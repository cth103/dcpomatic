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

/** @file  src/ffmpeg_decoder.cc
 *  @brief A decoder using FFmpeg to decode content.
 */

#include <stdexcept>
#include <vector>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <stdint.h>
#include <boost/lexical_cast.hpp>
extern "C" {
#include <tiffio.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libpostproc/postprocess.h>
}
#include <sndfile.h>
#include "film.h"
#include "format.h"
#include "transcoder.h"
#include "job.h"
#include "filter.h"
#include "options.h"
#include "exceptions.h"
#include "image.h"
#include "util.h"
#include "log.h"
#include "ffmpeg_decoder.h"
#include "filter_graph.h"
#include "subtitle.h"

#include "i18n.h"

using std::cout;
using std::string;
using std::vector;
using std::stringstream;
using std::list;
using boost::shared_ptr;
using boost::optional;
using boost::dynamic_pointer_cast;
using libdcp::Size;

FFmpegDecoder::FFmpegDecoder (shared_ptr<Film> f, DecodeOptions o)
	: Decoder (f, o)
	, VideoDecoder (f, o)
	, AudioDecoder (f, o)
	, _format_context (0)
	, _video_stream (-1)
	, _frame (0)
	, _video_codec_context (0)
	, _video_codec (0)
	, _audio_codec_context (0)
	, _audio_codec (0)
	, _subtitle_codec_context (0)
	, _subtitle_codec (0)
{
	setup_general ();
	setup_video ();
	setup_audio ();
	setup_subtitle ();

	if (!o.video_sync) {
		_first_video = 0;
	}
}

FFmpegDecoder::~FFmpegDecoder ()
{
	if (_audio_codec_context) {
		avcodec_close (_audio_codec_context);
	}
	
	if (_video_codec_context) {
		avcodec_close (_video_codec_context);
	}

	if (_subtitle_codec_context) {
		avcodec_close (_subtitle_codec_context);
	}

	av_free (_frame);
	
	avformat_close_input (&_format_context);
}	

void
FFmpegDecoder::setup_general ()
{
	av_register_all ();

	if (avformat_open_input (&_format_context, _film->content_path().c_str(), 0, 0) < 0) {
		throw OpenFileError (_film->content_path ());
	}

	if (avformat_find_stream_info (_format_context, 0) < 0) {
		throw DecodeError (_("could not find stream information"));
	}

	/* Find video, audio and subtitle streams and choose the first of each */

	for (uint32_t i = 0; i < _format_context->nb_streams; ++i) {
		AVStream* s = _format_context->streams[i];
		if (s->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			_video_stream = i;
		} else if (s->codec->codec_type == AVMEDIA_TYPE_AUDIO) {

			/* This is a hack; sometimes it seems that _audio_codec_context->channel_layout isn't set up,
			   so bodge it here.  No idea why we should have to do this.
			*/

			if (s->codec->channel_layout == 0) {
				s->codec->channel_layout = av_get_default_channel_layout (s->codec->channels);
			}
			
			_audio_streams.push_back (
				shared_ptr<AudioStream> (
					new FFmpegAudioStream (stream_name (s), i, s->codec->sample_rate, s->codec->channel_layout)
					)
				);
			
		} else if (s->codec->codec_type == AVMEDIA_TYPE_SUBTITLE) {
			_subtitle_streams.push_back (
				shared_ptr<SubtitleStream> (
					new SubtitleStream (stream_name (s), i)
					)
				);
		}
	}

	if (_video_stream < 0) {
		throw DecodeError (N_("could not find video stream"));
	}

	_frame = avcodec_alloc_frame ();
	if (_frame == 0) {
		throw DecodeError (N_("could not allocate frame"));
	}
}

void
FFmpegDecoder::setup_video ()
{
	_video_codec_context = _format_context->streams[_video_stream]->codec;
	_video_codec = avcodec_find_decoder (_video_codec_context->codec_id);

	if (_video_codec == 0) {
		throw DecodeError (_("could not find video decoder"));
	}

	if (avcodec_open2 (_video_codec_context, _video_codec, 0) < 0) {
		throw DecodeError (N_("could not open video decoder"));
	}
}

void
FFmpegDecoder::setup_audio ()
{
	if (!_audio_stream) {
		return;
	}

	shared_ptr<FFmpegAudioStream> ffa = dynamic_pointer_cast<FFmpegAudioStream> (_audio_stream);
	assert (ffa);
	
	_audio_codec_context = _format_context->streams[ffa->id()]->codec;
	_audio_codec = avcodec_find_decoder (_audio_codec_context->codec_id);

	if (_audio_codec == 0) {
		throw DecodeError (_("could not find audio decoder"));
	}

	if (avcodec_open2 (_audio_codec_context, _audio_codec, 0) < 0) {
		throw DecodeError (N_("could not open audio decoder"));
	}
}

void
FFmpegDecoder::setup_subtitle ()
{
	if (!_subtitle_stream || _subtitle_stream->id() >= int (_format_context->nb_streams)) {
		return;
	}

	_subtitle_codec_context = _format_context->streams[_subtitle_stream->id()]->codec;
	_subtitle_codec = avcodec_find_decoder (_subtitle_codec_context->codec_id);

	if (_subtitle_codec == 0) {
		throw DecodeError (_("could not find subtitle decoder"));
	}
	
	if (avcodec_open2 (_subtitle_codec_context, _subtitle_codec, 0) < 0) {
		throw DecodeError (N_("could not open subtitle decoder"));
	}
}


bool
FFmpegDecoder::pass ()
{
	int r = av_read_frame (_format_context, &_packet);
	
	if (r < 0) {
		if (r != AVERROR_EOF) {
			/* Maybe we should fail here, but for now we'll just finish off instead */
			char buf[256];
			av_strerror (r, buf, sizeof(buf));
			_film->log()->log (String::compose (N_("error on av_read_frame (%1) (%2)"), buf, r));
		}
		
		/* Get any remaining frames */
		
		_packet.data = 0;
		_packet.size = 0;

		/* XXX: should we reset _packet.data and size after each *_decode_* call? */

		int frame_finished;

		if (_opt.decode_video) {
			while (avcodec_decode_video2 (_video_codec_context, _frame, &frame_finished, &_packet) >= 0 && frame_finished) {
				filter_and_emit_video (_frame);
			}
		}

		if (_audio_stream && _opt.decode_audio) {
			decode_audio_packet ();
		}

		return true;
	}

	avcodec_get_frame_defaults (_frame);

	shared_ptr<FFmpegAudioStream> ffa = dynamic_pointer_cast<FFmpegAudioStream> (_audio_stream);

	if (_packet.stream_index == _video_stream && _opt.decode_video) {

		int frame_finished;
		int const r = avcodec_decode_video2 (_video_codec_context, _frame, &frame_finished, &_packet);
		if (r >= 0 && frame_finished) {

			if (r != _packet.size) {
				_film->log()->log (String::compose (N_("Used only %1 bytes of %2 in packet"), r, _packet.size));
			}

			if (_opt.video_sync) {
				out_with_sync ();
			} else {
				filter_and_emit_video (_frame);
			}
		}

	} else if (ffa && _packet.stream_index == ffa->id() && _opt.decode_audio) {
		decode_audio_packet ();
	} else if (_subtitle_stream && _packet.stream_index == _subtitle_stream->id() && _opt.decode_subtitles && _first_video) {

		int got_subtitle;
		AVSubtitle sub;
		if (avcodec_decode_subtitle2 (_subtitle_codec_context, &sub, &got_subtitle, &_packet) && got_subtitle) {
			/* Sometimes we get an empty AVSubtitle, which is used by some codecs to
			   indicate that the previous subtitle should stop.
			*/
			if (sub.num_rects > 0) {
				shared_ptr<TimedSubtitle> ts;
				try {
					emit_subtitle (shared_ptr<TimedSubtitle> (new TimedSubtitle (sub)));
				} catch (...) {
					/* some problem with the subtitle; we probably didn't understand it */
				}
			} else {
				emit_subtitle (shared_ptr<TimedSubtitle> ());
			}
			avsubtitle_free (&sub);
		}
	}
	
	av_free_packet (&_packet);
	return false;
}

/** @param data pointer to array of pointers to buffers.
 *  Only the first buffer will be used for non-planar data, otherwise there will be one per channel.
 */
shared_ptr<AudioBuffers>
FFmpegDecoder::deinterleave_audio (uint8_t** data, int size)
{
	assert (_film->audio_channels());
	assert (bytes_per_audio_sample());

	shared_ptr<FFmpegAudioStream> ffa = dynamic_pointer_cast<FFmpegAudioStream> (_audio_stream);
	assert (ffa);
	
	/* Deinterleave and convert to float */

	assert ((size % (bytes_per_audio_sample() * ffa->channels())) == 0);

	int const total_samples = size / bytes_per_audio_sample();
	int const frames = total_samples / _film->audio_channels();
	shared_ptr<AudioBuffers> audio (new AudioBuffers (ffa->channels(), frames));

	switch (audio_sample_format()) {
	case AV_SAMPLE_FMT_S16:
	{
		int16_t* p = reinterpret_cast<int16_t *> (data[0]);
		int sample = 0;
		int channel = 0;
		for (int i = 0; i < total_samples; ++i) {
			audio->data(channel)[sample] = float(*p++) / (1 << 15);

			++channel;
			if (channel == _film->audio_channels()) {
				channel = 0;
				++sample;
			}
		}
	}
	break;

	case AV_SAMPLE_FMT_S16P:
	{
		int16_t** p = reinterpret_cast<int16_t **> (data);
		for (int i = 0; i < _film->audio_channels(); ++i) {
			for (int j = 0; j < frames; ++j) {
				audio->data(i)[j] = static_cast<float>(p[i][j]) / (1 << 15);
			}
		}
	}
	break;
	
	case AV_SAMPLE_FMT_S32:
	{
		int32_t* p = reinterpret_cast<int32_t *> (data[0]);
		int sample = 0;
		int channel = 0;
		for (int i = 0; i < total_samples; ++i) {
			audio->data(channel)[sample] = static_cast<float>(*p++) / (1 << 31);

			++channel;
			if (channel == _film->audio_channels()) {
				channel = 0;
				++sample;
			}
		}
	}
	break;

	case AV_SAMPLE_FMT_FLT:
	{
		float* p = reinterpret_cast<float*> (data[0]);
		int sample = 0;
		int channel = 0;
		for (int i = 0; i < total_samples; ++i) {
			audio->data(channel)[sample] = *p++;

			++channel;
			if (channel == _film->audio_channels()) {
				channel = 0;
				++sample;
			}
		}
	}
	break;
		
	case AV_SAMPLE_FMT_FLTP:
	{
		float** p = reinterpret_cast<float**> (data);
		for (int i = 0; i < _film->audio_channels(); ++i) {
			memcpy (audio->data(i), p[i], frames * sizeof(float));
		}
	}
	break;

	default:
		throw DecodeError (String::compose (_("Unrecognised audio sample format (%1)"), static_cast<int> (audio_sample_format())));
	}

	return audio;
}

float
FFmpegDecoder::frames_per_second () const
{
	AVStream* s = _format_context->streams[_video_stream];

	if (s->avg_frame_rate.num && s->avg_frame_rate.den) {
		return av_q2d (s->avg_frame_rate);
	}

	return av_q2d (s->r_frame_rate);
}

AVSampleFormat
FFmpegDecoder::audio_sample_format () const
{
	if (_audio_codec_context == 0) {
		return (AVSampleFormat) 0;
	}
	
	return _audio_codec_context->sample_fmt;
}

libdcp::Size
FFmpegDecoder::native_size () const
{
	return libdcp::Size (_video_codec_context->width, _video_codec_context->height);
}

PixelFormat
FFmpegDecoder::pixel_format () const
{
	return _video_codec_context->pix_fmt;
}

int
FFmpegDecoder::time_base_numerator () const
{
	return _video_codec_context->time_base.num;
}

int
FFmpegDecoder::time_base_denominator () const
{
	return _video_codec_context->time_base.den;
}

int
FFmpegDecoder::sample_aspect_ratio_numerator () const
{
	return _video_codec_context->sample_aspect_ratio.num;
}

int
FFmpegDecoder::sample_aspect_ratio_denominator () const
{
	return _video_codec_context->sample_aspect_ratio.den;
}

string
FFmpegDecoder::stream_name (AVStream* s) const
{
	stringstream n;
	
	AVDictionaryEntry const * lang = av_dict_get (s->metadata, N_("language"), 0, 0);
	if (lang) {
		n << lang->value;
	}
	
	AVDictionaryEntry const * title = av_dict_get (s->metadata, N_("title"), 0, 0);
	if (title) {
		if (!n.str().empty()) {
			n << N_(" ");
		}
		n << title->value;
	}

	if (n.str().empty()) {
		n << N_("unknown");
	}

	return n.str ();
}

int
FFmpegDecoder::bytes_per_audio_sample () const
{
	return av_get_bytes_per_sample (audio_sample_format ());
}

void
FFmpegDecoder::set_audio_stream (shared_ptr<AudioStream> s)
{
	AudioDecoder::set_audio_stream (s);
	setup_audio ();
}

void
FFmpegDecoder::set_subtitle_stream (shared_ptr<SubtitleStream> s)
{
	VideoDecoder::set_subtitle_stream (s);
	setup_subtitle ();
	OutputChanged ();
}

void
FFmpegDecoder::filter_and_emit_video (AVFrame* frame)
{
	boost::mutex::scoped_lock lm (_filter_graphs_mutex);
	
	shared_ptr<FilterGraph> graph;

	list<shared_ptr<FilterGraph> >::iterator i = _filter_graphs.begin();
	while (i != _filter_graphs.end() && !(*i)->can_process (libdcp::Size (frame->width, frame->height), (AVPixelFormat) frame->format)) {
		++i;
	}

	if (i == _filter_graphs.end ()) {
		graph.reset (new FilterGraph (_film, this, libdcp::Size (frame->width, frame->height), (AVPixelFormat) frame->format));
		_filter_graphs.push_back (graph);
		_film->log()->log (String::compose (N_("New graph for %1x%2, pixel format %3"), frame->width, frame->height, frame->format));
	} else {
		graph = *i;
	}

	list<shared_ptr<Image> > images = graph->process (frame);

	for (list<shared_ptr<Image> >::iterator i = images.begin(); i != images.end(); ++i) {
		emit_video (*i, frame_time ());
	}
}

bool
FFmpegDecoder::seek (double p)
{
	return do_seek (p, false);
}

bool
FFmpegDecoder::seek_to_last ()
{
	/* This AVSEEK_FLAG_BACKWARD in do_seek is a bit of a hack; without it, if we ask for a seek to the same place as last time
	   (used when we change decoder parameters and want to re-fetch the frame) we end up going forwards rather than
	   staying in the same place.
	*/
	return do_seek (last_source_time(), true);
}

bool
FFmpegDecoder::do_seek (double p, bool backwards)
{
	int64_t const vt = p / av_q2d (_format_context->streams[_video_stream]->time_base);

	int const r = av_seek_frame (_format_context, _video_stream, vt, backwards ? AVSEEK_FLAG_BACKWARD : 0);
	
	avcodec_flush_buffers (_video_codec_context);
	if (_subtitle_codec_context) {
		avcodec_flush_buffers (_subtitle_codec_context);
	}
	
	return r < 0;
}

shared_ptr<FFmpegAudioStream>
FFmpegAudioStream::create (string t, optional<int> v)
{
	if (!v) {
		/* version < 1; no type in the string, and there's only FFmpeg streams anyway */
		return shared_ptr<FFmpegAudioStream> (new FFmpegAudioStream (t, v));
	}

	stringstream s (t);
	string type;
	s >> type;
	if (type != N_("ffmpeg")) {
		return shared_ptr<FFmpegAudioStream> ();
	}

	return shared_ptr<FFmpegAudioStream> (new FFmpegAudioStream (t, v));
}

FFmpegAudioStream::FFmpegAudioStream (string t, optional<int> version)
{
	stringstream n (t);
	
	int name_index = 4;
	if (!version) {
		name_index = 2;
		int channels;
		n >> _id >> channels;
		_channel_layout = av_get_default_channel_layout (channels);
		_sample_rate = 0;
	} else {
		string type;
		/* Current (marked version 1) */
		n >> type >> _id >> _sample_rate >> _channel_layout;
		assert (type == N_("ffmpeg"));
	}

	for (int i = 0; i < name_index; ++i) {
		size_t const s = t.find (' ');
		if (s != string::npos) {
			t = t.substr (s + 1);
		}
	}

	_name = t;
}

string
FFmpegAudioStream::to_string () const
{
	return String::compose (N_("ffmpeg %1 %2 %3 %4"), _id, _sample_rate, _channel_layout, _name);
}

void
FFmpegDecoder::out_with_sync ()
{
	/* Where we are in the output, in seconds */
	double const out_pts_seconds = video_frame() / frames_per_second();
	
	/* Where we are in the source, in seconds */
	double const source_pts_seconds = av_q2d (_format_context->streams[_packet.stream_index]->time_base)
		* av_frame_get_best_effort_timestamp(_frame);
	
	_film->log()->log (
		String::compose (N_("Source video frame ready; source at %1, output at %2"), source_pts_seconds, out_pts_seconds),
		Log::VERBOSE
		);
	
	if (!_first_video) {
		_first_video = source_pts_seconds;
	}
	
	/* Difference between where we are and where we should be */
	double const delta = source_pts_seconds - _first_video.get() - out_pts_seconds;
	double const one_frame = 1 / frames_per_second();
	
	/* Insert frames if required to get out_pts_seconds up to pts_seconds */
	if (delta > one_frame) {
		int const extra = rint (delta / one_frame);
		for (int i = 0; i < extra; ++i) {
			repeat_last_video ();
			_film->log()->log (
				String::compose (
					N_("Extra video frame inserted at %1s; source frame %2, source PTS %3 (at %4 fps)"),
					out_pts_seconds, video_frame(), source_pts_seconds, frames_per_second()
					)
				);
		}
	}
	
	if (delta > -one_frame) {
		/* Process this frame */
		filter_and_emit_video (_frame);
	} else {
		/* Otherwise we are omitting a frame to keep things right */
		_film->log()->log (String::compose (N_("Frame removed at %1s"), out_pts_seconds));
	}
}

void
FFmpegDecoder::film_changed (Film::Property p)
{
	switch (p) {
	case Film::CROP:
	case Film::FILTERS:
	{
		boost::mutex::scoped_lock lm (_filter_graphs_mutex);
		_filter_graphs.clear ();
	}
	OutputChanged ();
	break;

	default:
		break;
	}
}

/** @return Length (in video frames) according to our content's header */
SourceFrame
FFmpegDecoder::length () const
{
	return (double(_format_context->duration) / AV_TIME_BASE) * frames_per_second();
}

double
FFmpegDecoder::frame_time () const
{
	return av_frame_get_best_effort_timestamp(_frame) * av_q2d (_format_context->streams[_video_stream]->time_base);
}

void
FFmpegDecoder::decode_audio_packet ()
{
	shared_ptr<FFmpegAudioStream> ffa = dynamic_pointer_cast<FFmpegAudioStream> (_audio_stream);
	assert (ffa);

	/* Audio packets can contain multiple frames, so we may have to  call avcodec_decode_audio4
	   several times.
	*/
	
	AVPacket copy_packet = _packet;

	while (copy_packet.size > 0) {

		int frame_finished;
		int const decode_result = avcodec_decode_audio4 (_audio_codec_context, _frame, &frame_finished, &copy_packet);
		if (decode_result >= 0 && frame_finished) {
			
			/* Where we are in the source, in seconds */
			double const source_pts_seconds = av_q2d (_format_context->streams[copy_packet.stream_index]->time_base)
				* av_frame_get_best_effort_timestamp(_frame);
			
			/* We only decode audio if we've had our first video packet through, and if it
			   was before this packet.  Until then audio is thrown away.
			*/
			
			if ((_first_video && _first_video.get() <= source_pts_seconds) || !_opt.decode_video) {
				
				if (!_first_audio && _opt.decode_video) {
					_first_audio = source_pts_seconds;
					
					/* This is our first audio frame, and if we've arrived here we must have had our
					   first video frame.  Push some silence to make up any gap between our first
					   video frame and our first audio.
					*/
					
					/* frames of silence that we must push */
					int const s = rint ((_first_audio.get() - _first_video.get()) * ffa->sample_rate ());
					
					_film->log()->log (
						String::compose (
							N_("First video at %1, first audio at %2, pushing %3 audio frames of silence for %4 channels (%5 bytes per sample)"),
							_first_video.get(), _first_audio.get(), s, ffa->channels(), bytes_per_audio_sample()
							)
						);
					
					if (s) {
						shared_ptr<AudioBuffers> audio (new AudioBuffers (ffa->channels(), s));
						audio->make_silent ();
						Audio (audio);
					}
				}
				
				int const data_size = av_samples_get_buffer_size (
					0, _audio_codec_context->channels, _frame->nb_samples, audio_sample_format (), 1
					);
				
				assert (_audio_codec_context->channels == _film->audio_channels());
				Audio (deinterleave_audio (_frame->data, data_size));
			}
		}

		if (decode_result >= 0) {
			copy_packet.data += decode_result;
			copy_packet.size -= decode_result;
		}
	}
}
