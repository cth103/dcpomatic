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
#include "subtitle.h"

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
	if (_opt->decode_video_frequency != 0 && _fs->length() == 0) {
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
	_delay_in_bytes = _fs->total_audio_delay() * _fs->audio_sample_rate() * _fs->audio_channels() * bytes_per_audio_sample() / 1000;
	delete _delay_line;
	_delay_line = new DelayLine (_delay_in_bytes);

	_log->log (String::compose ("Decoding audio with total delay of %1", _fs->total_audio_delay()));

	_audio_frames_processed = 0;
}

/** Finish off a decode processing run */
void
Decoder::process_end ()
{
	if (_delay_in_bytes < 0) {
		uint8_t remainder[-_delay_in_bytes];
		_delay_line->get_remaining (remainder);
		_audio_frames_processed += _delay_in_bytes / (_fs->audio_channels() * bytes_per_audio_sample());
		emit_audio (remainder, -_delay_in_bytes);
	}

	/* If we cut the decode off, the audio may be short; push some silence
	   in to get it to the right length.
	*/

	int64_t const video_length_in_audio_frames = ((int64_t) _fs->dcp_length() * _fs->target_sample_rate() / _fs->frames_per_second());
	int64_t const audio_short_by_frames = video_length_in_audio_frames - _audio_frames_processed;

	_log->log (
		String::compose ("DCP length is %1 (%2 audio frames); %3 frames of audio processed.",
				 _fs->dcp_length(),
				 video_length_in_audio_frames,
				 _audio_frames_processed)
		);
	
	if (audio_short_by_frames >= 0 && _opt->decode_audio) {

		_log->log (String::compose ("DCP length is %1; %2 frames of audio processed.", _fs->dcp_length(), _audio_frames_processed));
		_log->log (String::compose ("Adding %1 frames of silence to the end.", audio_short_by_frames));

		int64_t bytes = audio_short_by_frames * _fs->audio_channels() * bytes_per_audio_sample();
		
		int64_t const silence_size = 16 * 1024 * _fs->audio_channels() * bytes_per_audio_sample();
		uint8_t silence[silence_size];
		memset (silence, 0, silence_size);
		
		while (bytes) {
			int64_t const t = min (bytes, silence_size);
			emit_audio (silence, t);
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
			_job->set_progress (float (_video_frame) / _fs->dcp_length());
		}
	}

	process_end ();
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
	
	if (!_ignore_length && _video_frame >= _fs->dcp_length()) {
		return true;
	}

	return do_pass ();
}

/** Called by subclasses to tell the world that some audio data is ready
 *  @param data Audio data, in FilmState::audio_sample_format.
 *  @param size Number of bytes of data.
 */
void
Decoder::process_audio (uint8_t* data, int size)
{
	/* Push into the delay line */
	size = _delay_line->feed (data, size);

	emit_audio (data, size);
}

void
Decoder::emit_audio (uint8_t* data, int size)
{
	/* Deinterleave and convert to float */

	assert ((size % (bytes_per_audio_sample() * _fs->audio_channels())) == 0);

	int const total_samples = size / bytes_per_audio_sample();
	int const frames = total_samples / _fs->audio_channels();
	shared_ptr<AudioBuffers> audio (new AudioBuffers (_fs->audio_channels(), frames));

	switch (audio_sample_format()) {
	case AV_SAMPLE_FMT_S16:
	{
		int16_t* p = (int16_t *) data;
		int sample = 0;
		int channel = 0;
		for (int i = 0; i < total_samples; ++i) {
			audio->data(channel)[sample] = float(*p++) / (1 << 15);

			++channel;
			if (channel == _fs->audio_channels()) {
				channel = 0;
				++sample;
			}
		}
	}
	break;

	case AV_SAMPLE_FMT_S32:
	{
		int32_t* p = (int32_t *) data;
		int sample = 0;
		int channel = 0;
		for (int i = 0; i < total_samples; ++i) {
			audio->data(channel)[sample] = float(*p++) / (1 << 31);

			++channel;
			if (channel == _fs->audio_channels()) {
				channel = 0;
				++sample;
			}
		}
	}

	case AV_SAMPLE_FMT_FLTP:
	{
		float* p = reinterpret_cast<float*> (data);
		for (int i = 0; i < _fs->audio_channels(); ++i) {
			memcpy (audio->data(i), p, frames * sizeof(float));
			p += frames;
		}
	}
	break;

	default:
		assert (false);
	}

	/* Maybe apply gain */
	if (_fs->audio_gain() != 0) {
		float const linear_gain = pow (10, _fs->audio_gain() / 20);
		for (int i = 0; i < _fs->audio_channels(); ++i) {
			for (int j = 0; j < frames; ++j) {
				audio->data(i)[j] *= linear_gain;
			}
		}
	}

	/* Update the number of audio frames we've pushed to the encoder */
	_audio_frames_processed += frames;

	Audio (audio);
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
		gap = _fs->length() / _opt->decode_video_frequency;
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

			shared_ptr<Subtitle> sub;
			if (_timed_subtitle && _timed_subtitle->displayed_at (double (last_video_frame()) / rint (_fs->frames_per_second()))) {
				sub = _timed_subtitle->subtitle ();
			}

			TIMING ("Decoder emits %1", _video_frame);
			Video (image, _video_frame, sub);
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
		fs << crop_string (Position (_fs->crop().left, _fs->crop().top), size_after_crop);
	} else {
		size_after_crop = native_size ();
		fs << crop_string (Position (0, 0), size_after_crop);
	}

	string filters = Filter::ffmpeg_strings (_fs->filters()).first;
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

void
Decoder::process_subtitle (shared_ptr<TimedSubtitle> s)
{
	_timed_subtitle = s;
	
	if (_opt->apply_crop) {
		Position const p = _timed_subtitle->subtitle()->position ();
		_timed_subtitle->subtitle()->set_position (Position (p.x - _fs->crop().left, p.y - _fs->crop().top));
	}
}


int
Decoder::bytes_per_audio_sample () const
{
	return av_get_bytes_per_sample (audio_sample_format ());
}
