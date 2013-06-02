/* -*- c-basic-offset: 8; default-tab-width: 8; -*- */

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
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libpostproc/postprocess.h>
}
#include <sndfile.h>
#include "film.h"
#include "filter.h"
#include "exceptions.h"
#include "image.h"
#include "util.h"
#include "log.h"
#include "ffmpeg_decoder.h"
#include "filter_graph.h"
#include "subtitle.h"
#include "audio_buffers.h"

#include "i18n.h"

using std::cout;
using std::string;
using std::vector;
using std::stringstream;
using std::list;
using std::min;
using boost::shared_ptr;
using boost::optional;
using boost::dynamic_pointer_cast;
using libdcp::Size;

boost::mutex FFmpegDecoder::_mutex;

FFmpegDecoder::FFmpegDecoder (shared_ptr<const Film> f, shared_ptr<const FFmpegContent> c, bool video, bool audio, bool subtitles)
	: Decoder (f)
	, VideoDecoder (f, c)
	, AudioDecoder (f, c)
	, _ffmpeg_content (c)
	, _format_context (0)
	, _video_stream (-1)
	, _frame (0)
	, _video_codec_context (0)
	, _video_codec (0)
	, _audio_codec_context (0)
	, _audio_codec (0)
	, _subtitle_codec_context (0)
	, _subtitle_codec (0)
	, _decode_video (video)
	, _decode_audio (audio)
	, _decode_subtitles (subtitles)
{
	setup_general ();
	setup_video ();
	setup_audio ();
	setup_subtitle ();
}

FFmpegDecoder::~FFmpegDecoder ()
{
	boost::mutex::scoped_lock lm (_mutex);
	
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

	if (avformat_open_input (&_format_context, _ffmpeg_content->file().string().c_str(), 0, 0) < 0) {
		throw OpenFileError (_ffmpeg_content->file().string ());
	}

	if (avformat_find_stream_info (_format_context, 0) < 0) {
		throw DecodeError (_("could not find stream information"));
	}

	/* Find video, audio and subtitle streams */

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
				shared_ptr<FFmpegAudioStream> (
					new FFmpegAudioStream (stream_name (s), i, s->codec->sample_rate, s->codec->channels)
					)
				);

		} else if (s->codec->codec_type == AVMEDIA_TYPE_SUBTITLE) {
			_subtitle_streams.push_back (shared_ptr<FFmpegSubtitleStream> (new FFmpegSubtitleStream (stream_name (s), i)));
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
	boost::mutex::scoped_lock lm (_mutex);
	
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
	boost::mutex::scoped_lock lm (_mutex);
	
	if (!_ffmpeg_content->audio_stream ()) {
		return;
	}

	_audio_codec_context = _format_context->streams[_ffmpeg_content->audio_stream()->id]->codec;
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
	boost::mutex::scoped_lock lm (_mutex);
	
	if (!_ffmpeg_content->subtitle_stream() || _ffmpeg_content->subtitle_stream()->id >= int (_format_context->nb_streams)) {
		return;
	}

	_subtitle_codec_context = _format_context->streams[_ffmpeg_content->subtitle_stream()->id]->codec;
	_subtitle_codec = avcodec_find_decoder (_subtitle_codec_context->codec_id);

	if (_subtitle_codec == 0) {
		throw DecodeError (_("could not find subtitle decoder"));
	}
	
	if (avcodec_open2 (_subtitle_codec_context, _subtitle_codec, 0) < 0) {
		throw DecodeError (N_("could not open subtitle decoder"));
	}
}


void
FFmpegDecoder::pass ()
{
	int r = av_read_frame (_format_context, &_packet);

	if (r < 0) {
		if (r != AVERROR_EOF) {
			/* Maybe we should fail here, but for now we'll just finish off instead */
			char buf[256];
			av_strerror (r, buf, sizeof(buf));
			shared_ptr<const Film> film = _film.lock ();
			assert (film);
			film->log()->log (String::compose (N_("error on av_read_frame (%1) (%2)"), buf, r));
		}

		/* Get any remaining frames */
		
		_packet.data = 0;
		_packet.size = 0;
		
		/* XXX: should we reset _packet.data and size after each *_decode_* call? */
		
		if (_decode_video) {
			while (decode_video_packet ());
		}

		if (_ffmpeg_content->audio_stream() && _decode_audio) {
			decode_audio_packet ();
		}
			
		return;
	}

	avcodec_get_frame_defaults (_frame);

	if (_packet.stream_index == _video_stream && _decode_video) {
		decode_video_packet ();
	} else if (_ffmpeg_content->audio_stream() && _packet.stream_index == _ffmpeg_content->audio_stream()->id && _decode_audio) {
		decode_audio_packet ();
	} else if (_ffmpeg_content->subtitle_stream() && _packet.stream_index == _ffmpeg_content->subtitle_stream()->id && _decode_subtitles) {

		int got_subtitle;
		AVSubtitle sub;
		if (avcodec_decode_subtitle2 (_subtitle_codec_context, &sub, &got_subtitle, &_packet) && got_subtitle) {
			/* Sometimes we get an empty AVSubtitle, which is used by some codecs to
			   indicate that the previous subtitle should stop.
			*/
			if (sub.num_rects > 0) {
				shared_ptr<TimedSubtitle> ts;
				try {
					subtitle (shared_ptr<TimedSubtitle> (new TimedSubtitle (sub)));
				} catch (...) {
					/* some problem with the subtitle; we probably didn't understand it */
				}
			} else {
				subtitle (shared_ptr<TimedSubtitle> ());
			}
			avsubtitle_free (&sub);
		}
	}

	av_free_packet (&_packet);
}

/** @param data pointer to array of pointers to buffers.
 *  Only the first buffer will be used for non-planar data, otherwise there will be one per channel.
 */
shared_ptr<AudioBuffers>
FFmpegDecoder::deinterleave_audio (uint8_t** data, int size)
{
	assert (_ffmpeg_content->audio_channels());
	assert (bytes_per_audio_sample());

	/* Deinterleave and convert to float */

	assert ((size % (bytes_per_audio_sample() * _ffmpeg_content->audio_channels())) == 0);

	int const total_samples = size / bytes_per_audio_sample();
	int const frames = total_samples / _ffmpeg_content->audio_channels();
	shared_ptr<AudioBuffers> audio (new AudioBuffers (_ffmpeg_content->audio_channels(), frames));

	switch (audio_sample_format()) {
	case AV_SAMPLE_FMT_S16:
	{
		int16_t* p = reinterpret_cast<int16_t *> (data[0]);
		int sample = 0;
		int channel = 0;
		for (int i = 0; i < total_samples; ++i) {
			audio->data(channel)[sample] = float(*p++) / (1 << 15);

			++channel;
			if (channel == _ffmpeg_content->audio_channels()) {
				channel = 0;
				++sample;
			}
		}
	}
	break;

	case AV_SAMPLE_FMT_S16P:
	{
		int16_t** p = reinterpret_cast<int16_t **> (data);
		for (int i = 0; i < _ffmpeg_content->audio_channels(); ++i) {
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
			if (channel == _ffmpeg_content->audio_channels()) {
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
			if (channel == _ffmpeg_content->audio_channels()) {
				channel = 0;
				++sample;
			}
		}
	}
	break;
		
	case AV_SAMPLE_FMT_FLTP:
	{
		float** p = reinterpret_cast<float**> (data);
		for (int i = 0; i < _ffmpeg_content->audio_channels(); ++i) {
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
FFmpegDecoder::video_frame_rate () const
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
FFmpegDecoder::video_size () const
{
	return libdcp::Size (_video_codec_context->width, _video_codec_context->height);
}

string
FFmpegDecoder::stream_name (AVStream* s) const
{
	stringstream n;

	if (s->metadata) {
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
FFmpegDecoder::seek (Time t)
{
	do_seek (t, false, false);
}

void
FFmpegDecoder::seek_back ()
{
	if (next() < (2.5 * TIME_HZ / video_frame_rate())) {
		return;
	}
	
	do_seek (next() - 2.5 * TIME_HZ / video_frame_rate(), true, true);
}

void
FFmpegDecoder::seek_forward ()
{
	if (next() >= (_ffmpeg_content->length() - 0.5 * TIME_HZ / video_frame_rate())) {
		return;
	}
	
	do_seek (next() - 0.5 * TIME_HZ / video_frame_rate(), true, true);
}

void
FFmpegDecoder::do_seek (Time t, bool backwards, bool accurate)
{
	int64_t const vt = t / (av_q2d (_format_context->streams[_video_stream]->time_base) * TIME_HZ);
	av_seek_frame (_format_context, _video_stream, vt, backwards ? AVSEEK_FLAG_BACKWARD : 0);

	avcodec_flush_buffers (_video_codec_context);
	if (_subtitle_codec_context) {
		avcodec_flush_buffers (_subtitle_codec_context);
	}

	if (accurate) {
		while (1) {
			int r = av_read_frame (_format_context, &_packet);
			if (r < 0) {
				return;
			}
			
			avcodec_get_frame_defaults (_frame);
			
			if (_packet.stream_index == _video_stream) {
				int finished = 0;
				int const r = avcodec_decode_video2 (_video_codec_context, _frame, &finished, &_packet);
				if (r >= 0 && finished) {
					int64_t const bet = av_frame_get_best_effort_timestamp (_frame);
					if (bet > vt) {
						break;
					}
				}
			}
			
			av_free_packet (&_packet);
		}
	}

	return;
}

/** @return Length (in video frames) according to our content's header */
ContentVideoFrame
FFmpegDecoder::video_length () const
{
	return (double(_format_context->duration) / AV_TIME_BASE) * video_frame_rate();
}

void
FFmpegDecoder::decode_audio_packet ()
{
	/* Audio packets can contain multiple frames, so we may have to call avcodec_decode_audio4
	   several times.
	*/
	
	AVPacket copy_packet = _packet;

	while (copy_packet.size > 0) {

		int frame_finished;
		int const decode_result = avcodec_decode_audio4 (_audio_codec_context, _frame, &frame_finished, &copy_packet);
		if (decode_result >= 0) {
			if (frame_finished) {
			
				/* Where we are in the source, in seconds */
				double const source_pts_seconds = av_q2d (_format_context->streams[copy_packet.stream_index]->time_base)
					* av_frame_get_best_effort_timestamp(_frame);
				
				int const data_size = av_samples_get_buffer_size (
					0, _audio_codec_context->channels, _frame->nb_samples, audio_sample_format (), 1
					);
				
				assert (_audio_codec_context->channels == _ffmpeg_content->audio_channels());
				audio (deinterleave_audio (_frame->data, data_size), source_pts_seconds);
			}
			
			copy_packet.data += decode_result;
			copy_packet.size -= decode_result;
		}
	}
}

bool
FFmpegDecoder::decode_video_packet ()
{
	int frame_finished;
	if (avcodec_decode_video2 (_video_codec_context, _frame, &frame_finished, &_packet) < 0 || !frame_finished) {
		return false;
	}
		
	boost::mutex::scoped_lock lm (_filter_graphs_mutex);

	shared_ptr<FilterGraph> graph;
	
	list<shared_ptr<FilterGraph> >::iterator i = _filter_graphs.begin();
	while (i != _filter_graphs.end() && !(*i)->can_process (libdcp::Size (_frame->width, _frame->height), (AVPixelFormat) _frame->format)) {
		++i;
	}

	if (i == _filter_graphs.end ()) {
		shared_ptr<const Film> film = _film.lock ();
		assert (film);

		graph.reset (new FilterGraph (_ffmpeg_content, libdcp::Size (_frame->width, _frame->height), (AVPixelFormat) _frame->format));
		_filter_graphs.push_back (graph);

		film->log()->log (String::compose (N_("New graph for %1x%2, pixel format %3"), _frame->width, _frame->height, _frame->format));
	} else {
		graph = *i;
	}

	list<shared_ptr<Image> > images = graph->process (_frame);

	string post_process = Filter::ffmpeg_strings (_ffmpeg_content->filters()).second;
	
	for (list<shared_ptr<Image> >::iterator i = images.begin(); i != images.end(); ++i) {

		shared_ptr<Image> image = *i;
		if (!post_process.empty ()) {
			image = image->post_process (post_process, true);
		}
		
		int64_t const bet = av_frame_get_best_effort_timestamp (_frame);
		if (bet != AV_NOPTS_VALUE) {
			/* XXX: may need to insert extra frames / remove frames here ...
			   (as per old Matcher)
			*/
			Time const t = bet * av_q2d (_format_context->streams[_video_stream]->time_base) * TIME_HZ;
			video (image, false, t);
		} else {
			shared_ptr<const Film> film = _film.lock ();
			assert (film);
			film->log()->log ("Dropping frame without PTS");
		}
	}

	return true;
}

Time
FFmpegDecoder::next () const
{
	if (_decode_video && _decode_audio && _audio_codec_context) {
		return min (_next_video, _next_audio);
	}

	if (_decode_audio && _audio_codec_context) {
		return _next_audio;
	}

	return _next_video;
}

bool
FFmpegDecoder::done () const
{
	return (!_decode_audio || !_audio_codec_context || audio_done()) && (!_decode_video || video_done());
}
	
