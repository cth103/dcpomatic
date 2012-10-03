/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/** @file  src/decoder.cc
 *  @brief Parent class for decoders of content.
 */

#include <iostream>
#include <stdint.h>
#include <boost/lexical_cast.hpp>
extern "C" {
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersrc.h>
#if (LIBAVFILTER_VERSION_MAJOR == 2 && LIBAVFILTER_VERSION_MINOR >= 53 && LIBAVFILTER_VERSION_MINOR <= 77) || LIBAVFILTER_VERSION_MAJOR == 3
#include <libavfilter/avcodec.h>
#include <libavfilter/buffersink.h>
#elif LIBAVFILTER_VERSION_MAJOR == 2 && LIBAVFILTER_VERSION_MINOR == 15
#include <libavfilter/vsrc_buffer.h>
#endif
#include <libavformat/avio.h>
}
#include "film.h"
#include "format.h"
#include "job.h"
#include "film_state.h"
#include "options.h"
#include "exceptions.h"
#include "image.h"
#include "util.h"
#include "log.h"
#include "decoder.h"
#include "filter.h"
#include "delay_line.h"
#include "ffmpeg_compatibility.h"

using namespace std;
using namespace boost;

/** @param s FilmState of the Film.
 *  @param o Options.
 *  @param j Job that we are running within, or 0
 *  @param l Log to use.
 *  @param minimal true to do the bare minimum of work; just run through the content.  Useful for acquiring
 *  accurate frame counts as quickly as possible.  This generates no video or audio output.
 *  @param ignore_length Ignore the content's claimed length when computing progress.
 */
Decoder::Decoder (boost::shared_ptr<const FilmState> s, boost::shared_ptr<const Options> o, Job* j, Log* l, bool minimal, bool ignore_length)
	: _fs (s)
	, _opt (o)
	, _job (j)
	, _log (l)
	, _minimal (minimal)
	, _ignore_length (ignore_length)
	, _video_frame (0)
	, _buffer_src_context (0)
	, _buffer_sink_context (0)
	, _have_setup_video_filters (false)
	, _delay_line (0)
	, _delay_in_bytes (0)
	, _audio_frames_processed (0)
{
	if (_opt->decode_video_frequency != 0 && _fs->length == 0) {
		throw DecodeError ("cannot do a partial decode if length == 0");
	}
}

Decoder::~Decoder ()
{
	delete _delay_line;
}

/** Start off a decode processing run */
void
Decoder::process_begin ()
{
	_delay_in_bytes = _fs->audio_delay * _fs->audio_sample_rate * _fs->audio_channels * _fs->bytes_per_sample() / 1000;
	delete _delay_line;
	_delay_line = new DelayLine (_delay_in_bytes);

	_audio_frames_processed = 0;
}

/** Finish off a decode processing run */
void
Decoder::process_end ()
{
	if (_delay_in_bytes < 0) {
		uint8_t remainder[-_delay_in_bytes];
		_delay_line->get_remaining (remainder);
		_audio_frames_processed += _delay_in_bytes / (_fs->audio_channels * _fs->bytes_per_sample());
		Audio (remainder, _delay_in_bytes);
	}

	/* If we cut the decode off, the audio may be short; push some silence
	   in to get it to the right length.
	*/

	int64_t const audio_short_by_frames =
		((int64_t) decoding_frames() * _fs->target_sample_rate() / _fs->frames_per_second)
		- _audio_frames_processed;

	if (audio_short_by_frames >= 0) {

		stringstream s;
		s << "Adding " << audio_short_by_frames << " frames of silence to the end.";
		_log->log (s.str ());

		int64_t bytes = audio_short_by_frames * _fs->audio_channels * _fs->bytes_per_sample();
		
		int64_t const silence_size = 64 * 1024;
		uint8_t silence[silence_size];
		memset (silence, 0, silence_size);
		
		while (bytes) {
			int64_t const t = min (bytes, silence_size);
			Audio (silence, t);
			bytes -= t;
		}
	}
}

/** Start decoding */
void
Decoder::go ()
{
	process_begin ();

	if (_job && _ignore_length) {
		_job->set_progress_unknown ();
	}

	while (pass () == false) {
		if (_job && !_ignore_length) {
			_job->set_progress (float (_video_frame) / decoding_frames ());
		}
	}

	process_end ();
}

/** @return Number of frames that we will be decoding */
int
Decoder::decoding_frames () const
{
	if (_opt->num_frames > 0) {
		return _opt->num_frames;
	}
	
	return _fs->length;
}

/** Run one pass.  This may or may not generate any actual video / audio data;
 *  some decoders may require several passes to generate a single frame.
 *  @return true if we have finished processing all data; otherwise false.
 */
bool
Decoder::pass ()
{
	if (!_have_setup_video_filters) {
		setup_video_filters ();
		_have_setup_video_filters = true;
	}
	
	if (_opt->num_frames != 0 && _video_frame >= _opt->num_frames) {
		return true;
	}

	return do_pass ();
}

/** Called by subclasses to tell the world that some audio data is ready
 *  @param data Interleaved audio data, in FilmState::audio_sample_format.
 *  @param size Number of bytes of data.
 */
void
Decoder::process_audio (uint8_t* data, int size)
{
	/* Samples per channel */
	int const samples = size / _fs->bytes_per_sample();

	/* Maybe apply gain */
	if (_fs->audio_gain != 0) {
		float const linear_gain = pow (10, _fs->audio_gain / 20);
		uint8_t* p = data;
		switch (_fs->audio_sample_format) {
		case AV_SAMPLE_FMT_S16:
			for (int i = 0; i < samples; ++i) {
				/* XXX: assumes little-endian; also we should probably be dithering here */

				/* unsigned sample */
				int const ou = p[0] | (p[1] << 8);

				/* signed sample */
				int const os = ou >= 0x8000 ? (- 0x10000 + ou) : ou;

				/* signed sample with altered gain */
				int const gs = int (os * linear_gain);

				/* unsigned sample with altered gain */
				int const gu = gs > 0 ? gs : (0x10000 + gs);

				/* write it back */
				p[0] = gu & 0xff;
				p[1] = (gu & 0xff00) >> 8;
				p += 2;
			}
			break;
		default:
			assert (false);
		}
	}

	/* Update the number of audio frames we've pushed to the encoder */
	_audio_frames_processed += size / (_fs->audio_channels * _fs->bytes_per_sample ());

	/* Push into the delay line and then tell the world what we've got */
	int available = _delay_line->feed (data, size);
	Audio (data, available);
}

/** Called by subclasses to tell the world that some video data is ready.
 *  We do some post-processing / filtering then emit it for listeners.
 *  @param frame to decode; caller manages memory.
 */
void
Decoder::process_video (AVFrame* frame)
{
	if (_minimal) {
		++_video_frame;
		return;
	}

	/* Use FilmState::length here as our one may be wrong */

	int gap = 0;
	if (_opt->decode_video_frequency != 0) {
		gap = _fs->length / _opt->decode_video_frequency;
	}

	if (_opt->decode_video_frequency != 0 && gap != 0 && (_video_frame % gap) != 0) {
		++_video_frame;
		return;
	}

#if LIBAVFILTER_VERSION_MAJOR == 2 && LIBAVFILTER_VERSION_MINOR >= 53 && LIBAVFILTER_VERSION_MINOR <= 61

	if (av_vsrc_buffer_add_frame (_buffer_src_context, frame, 0) < 0) {
		throw DecodeError ("could not push buffer into filter chain.");
	}

#elif LIBAVFILTER_VERSION_MAJOR == 2 && LIBAVFILTER_VERSION_MINOR == 15

	AVRational par;
	par.num = sample_aspect_ratio_numerator ();
	par.den = sample_aspect_ratio_denominator ();

	if (av_vsrc_buffer_add_frame (_buffer_src_context, frame, 0, par) < 0) {
		throw DecodeError ("could not push buffer into filter chain.");
	}

#else

	if (av_buffersrc_write_frame (_buffer_src_context, frame) < 0) {
		throw DecodeError ("could not push buffer into filter chain.");
	}

#endif	
	
#if LIBAVFILTER_VERSION_MAJOR == 2 && LIBAVFILTER_VERSION_MINOR >= 15 && LIBAVFILTER_VERSION_MINOR <= 61	
	while (avfilter_poll_frame (_buffer_sink_context->inputs[0])) {
#else
	while (av_buffersink_read (_buffer_sink_context, 0)) {
#endif		

#if LIBAVFILTER_VERSION_MAJOR == 2 && LIBAVFILTER_VERSION_MINOR >= 15
		
		int r = avfilter_request_frame (_buffer_sink_context->inputs[0]);
		if (r < 0) {
			throw DecodeError ("could not request filtered frame");
		}
		
		AVFilterBufferRef* filter_buffer = _buffer_sink_context->inputs[0]->cur_buf;
		
#else

		AVFilterBufferRef* filter_buffer;
		if (av_buffersink_get_buffer_ref (_buffer_sink_context, &filter_buffer, 0) < 0) {
			filter_buffer = 0;
		}

#endif		
		
		if (filter_buffer) {
			/* This takes ownership of filter_buffer */
			shared_ptr<Image> image (new FilterBufferImage ((PixelFormat) frame->format, filter_buffer));

			if (_opt->black_after > 0 && _video_frame > _opt->black_after) {
				image->make_black ();
			}

			TIMING ("Decoder emits %1", _video_frame);
			Video (image, _video_frame);
			++_video_frame;
		}
	}
}


/** Set up a video filtering chain to include cropping and any filters that are specified
 *  by the Film.
 */
void
Decoder::setup_video_filters ()
{
	stringstream fs;
	Size size_after_crop;
	
	if (_opt->apply_crop) {
		size_after_crop = _fs->cropped_size (native_size ());
		fs << crop_string (Position (_fs->crop.left, _fs->crop.top), size_after_crop);
	} else {
		size_after_crop = native_size ();
		fs << crop_string (Position (0, 0), size_after_crop);
	}

	string filters = Filter::ffmpeg_strings (_fs->filters).first;
	if (!filters.empty ()) {
		filters += ",";
	}

	filters += fs.str ();

	avfilter_register_all ();
	
	AVFilterGraph* graph = avfilter_graph_alloc();
	if (graph == 0) {
		throw DecodeError ("Could not create filter graph.");
	}

	AVFilter* buffer_src = avfilter_get_by_name("buffer");
	if (buffer_src == 0) {
		throw DecodeError ("Could not find buffer src filter");
	}

	AVFilter* buffer_sink = get_sink ();

	stringstream a;
	a << native_size().width << ":"
	  << native_size().height << ":"
	  << pixel_format() << ":"
	  << time_base_numerator() << ":"
	  << time_base_denominator() << ":"
	  << sample_aspect_ratio_numerator() << ":"
	  << sample_aspect_ratio_denominator();

	int r;

	if ((r = avfilter_graph_create_filter (&_buffer_src_context, buffer_src, "in", a.str().c_str(), 0, graph)) < 0) {
		throw DecodeError ("could not create buffer source");
	}

	AVBufferSinkParams* sink_params = av_buffersink_params_alloc ();
	PixelFormat* pixel_fmts = new PixelFormat[2];
	pixel_fmts[0] = pixel_format ();
	pixel_fmts[1] = PIX_FMT_NONE;
	sink_params->pixel_fmts = pixel_fmts;
	
	if (avfilter_graph_create_filter (&_buffer_sink_context, buffer_sink, "out", 0, sink_params, graph) < 0) {
		throw DecodeError ("could not create buffer sink.");
	}

	AVFilterInOut* outputs = avfilter_inout_alloc ();
	outputs->name = av_strdup("in");
	outputs->filter_ctx = _buffer_src_context;
	outputs->pad_idx = 0;
	outputs->next = 0;

	AVFilterInOut* inputs = avfilter_inout_alloc ();
	inputs->name = av_strdup("out");
	inputs->filter_ctx = _buffer_sink_context;
	inputs->pad_idx = 0;
	inputs->next = 0;

	_log->log ("Using filter chain `" + filters + "'");

#if LIBAVFILTER_VERSION_MAJOR == 2 && LIBAVFILTER_VERSION_MINOR == 15
	if (avfilter_graph_parse (graph, filters.c_str(), inputs, outputs, 0) < 0) {
		throw DecodeError ("could not set up filter graph.");
	}
#else	
	if (avfilter_graph_parse (graph, filters.c_str(), &inputs, &outputs, 0) < 0) {
		throw DecodeError ("could not set up filter graph.");
	}
#endif	
	
	if (avfilter_graph_config (graph, 0) < 0) {
		throw DecodeError ("could not configure filter graph.");
	}

	/* XXX: leaking `inputs' / `outputs' ? */
}

