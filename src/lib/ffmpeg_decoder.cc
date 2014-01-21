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
#include <sndfile.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include "film.h"
#include "filter.h"
#include "exceptions.h"
#include "image.h"
#include "util.h"
#include "log.h"
#include "ffmpeg_decoder.h"
#include "filter_graph.h"
#include "audio_buffers.h"
#include "ffmpeg_content.h"

#include "i18n.h"

using std::cout;
using std::string;
using std::vector;
using std::stringstream;
using std::list;
using std::min;
using std::pair;
using boost::shared_ptr;
using boost::optional;
using boost::dynamic_pointer_cast;
using libdcp::Size;

FFmpegDecoder::FFmpegDecoder (shared_ptr<const Film> f, shared_ptr<const FFmpegContent> c, bool video, bool audio)
	: Decoder (f)
	, VideoDecoder (f, c)
	, AudioDecoder (f, c)
	, SubtitleDecoder (f)
	, FFmpeg (c)
	, _subtitle_codec_context (0)
	, _subtitle_codec (0)
	, _decode_video (video)
	, _decode_audio (audio)
	, _pts_offset (0)
	, _just_sought (false)
{
	setup_subtitle ();

	/* Audio and video frame PTS values may not start with 0.  We want
	   to fiddle them so that:

	   1.  One of them starts at time 0.
	   2.  The first video PTS value ends up on a frame boundary.

	   Then we remove big initial gaps in PTS and we allow our
	   insertion of black frames to work.

	   We will do:
	     audio_pts_to_use = audio_pts_from_ffmpeg + pts_offset;
	     video_pts_to_use = video_pts_from_ffmpeg + pts_offset;
	*/

	bool const have_video = video && c->first_video();
	bool const have_audio = audio && c->audio_stream() && c->audio_stream()->first_audio;

	/* First, make one of them start at 0 */

	if (have_audio && have_video) {
		_pts_offset = - min (c->first_video().get(), c->audio_stream()->first_audio.get());
	} else if (have_video) {
		_pts_offset = - c->first_video().get();
	} else if (have_audio) {
		_pts_offset = - c->audio_stream()->first_audio.get();
	}

	/* Now adjust both so that the video pts starts on a frame */
	if (have_video && have_audio) {
		double first_video = c->first_video().get() + _pts_offset;
		double const old_first_video = first_video;
		
		/* Round the first video up to a frame boundary */
		if (fabs (rint (first_video * c->video_frame_rate()) - first_video * c->video_frame_rate()) > 1e-6) {
			first_video = ceil (first_video * c->video_frame_rate()) / c->video_frame_rate ();
		}

		_pts_offset += first_video - old_first_video;
	}
}

FFmpegDecoder::~FFmpegDecoder ()
{
	boost::mutex::scoped_lock lm (_mutex);

	if (_subtitle_codec_context) {
		avcodec_close (_subtitle_codec_context);
	}
}

void
FFmpegDecoder::flush ()
{
	/* Get any remaining frames */
	
	_packet.data = 0;
	_packet.size = 0;
	
	/* XXX: should we reset _packet.data and size after each *_decode_* call? */
	
	if (_decode_video) {
		while (decode_video_packet ()) {}
	}
	
	if (_ffmpeg_content->audio_stream() && _decode_audio) {
		decode_audio_packet ();
	}

	/* Stop us being asked for any more data */
	_video_position = _ffmpeg_content->video_length ();
	_audio_position = _ffmpeg_content->audio_length ();
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

		flush ();
		return;
	}

	shared_ptr<const Film> film = _film.lock ();
	assert (film);

	int const si = _packet.stream_index;
	
	if (si == _video_stream && _decode_video) {
		decode_video_packet ();
	} else if (_ffmpeg_content->audio_stream() && si == _ffmpeg_content->audio_stream()->index (_format_context) && _decode_audio) {
		decode_audio_packet ();
	} else if (_ffmpeg_content->subtitle_stream() && si == _ffmpeg_content->subtitle_stream()->index (_format_context) && film->with_subtitles ()) {
		decode_subtitle_packet ();
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

AVSampleFormat
FFmpegDecoder::audio_sample_format () const
{
	if (!_ffmpeg_content->audio_stream()) {
		return (AVSampleFormat) 0;
	}
	
	return audio_codec_context()->sample_fmt;
}

int
FFmpegDecoder::bytes_per_audio_sample () const
{
	return av_get_bytes_per_sample (audio_sample_format ());
}

void
FFmpegDecoder::seek (VideoContent::Frame frame, bool accurate)
{
	double const time_base = av_q2d (_format_context->streams[_video_stream]->time_base);

	/* If we are doing an accurate seek, our initial shot will be 5 frames (5 being
	   a number plucked from the air) earlier than we want to end up.  The loop below
	   will hopefully then step through to where we want to be.
	*/
	int initial = frame;

	if (accurate) {
		initial -= 5;
	}

	if (initial < 0) {
		initial = 0;
	}

	/* Initial seek time in the stream's timebase */
	int64_t const initial_vt = ((initial / _ffmpeg_content->video_frame_rate()) - _pts_offset) / time_base;

	av_seek_frame (_format_context, _video_stream, initial_vt, AVSEEK_FLAG_BACKWARD);

	avcodec_flush_buffers (video_codec_context());
	if (_subtitle_codec_context) {
		avcodec_flush_buffers (_subtitle_codec_context);
	}

	/* This !accurate is piling hack upon hack; setting _just_sought to true
	   even with accurate == true defeats our attempt to align the start
	   of the video and audio.  Here we disable that defeat when accurate == true
	   i.e. when we are making a DCP rather than just previewing one.
	   Ewww.  This should be gone in 2.0.
	*/
	if (!accurate) {
		_just_sought = true;
	}
	
	_video_position = frame;
	
	if (frame == 0 || !accurate) {
		/* We're already there, or we're as close as we need to be */
		return;
	}

	while (1) {
		int r = av_read_frame (_format_context, &_packet);
		if (r < 0) {
			return;
		}

		if (_packet.stream_index != _video_stream) {
			av_free_packet (&_packet);
			continue;
		}
		
		int finished = 0;
		r = avcodec_decode_video2 (video_codec_context(), _frame, &finished, &_packet);
		if (r >= 0 && finished) {
			_video_position = rint (
				(av_frame_get_best_effort_timestamp (_frame) * time_base + _pts_offset) * _ffmpeg_content->video_frame_rate()
				);

			if (_video_position >= (frame - 1)) {
				av_free_packet (&_packet);
				break;
			}
		}
		
		av_free_packet (&_packet);
	}
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
		int const decode_result = avcodec_decode_audio4 (audio_codec_context(), _frame, &frame_finished, &copy_packet);
		if (decode_result < 0) {
			shared_ptr<const Film> film = _film.lock ();
			assert (film);
			film->log()->log (String::compose ("avcodec_decode_audio4 failed (%1)", decode_result));
			return;
		}

		if (frame_finished) {
			
			if (_audio_position == 0) {
				/* Where we are in the source, in seconds */
				double const pts = av_q2d (_format_context->streams[copy_packet.stream_index]->time_base)
					* av_frame_get_best_effort_timestamp(_frame) + _pts_offset;

				if (pts > 0) {
					/* Emit some silence */
					shared_ptr<AudioBuffers> silence (
						new AudioBuffers (
							_ffmpeg_content->audio_channels(),
							pts * _ffmpeg_content->content_audio_frame_rate()
							)
						);
					
					silence->make_silent ();
					audio (silence, _audio_position);
				}
			}
			
			int const data_size = av_samples_get_buffer_size (
				0, audio_codec_context()->channels, _frame->nb_samples, audio_sample_format (), 1
				);
			
			audio (deinterleave_audio (_frame->data, data_size), _audio_position);
		}
			
		copy_packet.data += decode_result;
		copy_packet.size -= decode_result;
	}
}

bool
FFmpegDecoder::decode_video_packet ()
{
	int frame_finished;
	if (avcodec_decode_video2 (video_codec_context(), _frame, &frame_finished, &_packet) < 0 || !frame_finished) {
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

	list<pair<shared_ptr<Image>, int64_t> > images = graph->process (_frame);

	string post_process = Filter::ffmpeg_strings (_ffmpeg_content->filters()).second;
	
	for (list<pair<shared_ptr<Image>, int64_t> >::iterator i = images.begin(); i != images.end(); ++i) {

		shared_ptr<Image> image = i->first;
		if (!post_process.empty ()) {
			image = image->post_process (post_process, true);
		}
		
		if (i->second != AV_NOPTS_VALUE) {

			double const pts = i->second * av_q2d (_format_context->streams[_video_stream]->time_base) + _pts_offset;

			if (_just_sought) {
				/* We just did a seek, so disable any attempts to correct for where we
				   are / should be.
				*/
				_video_position = rint (pts * _ffmpeg_content->video_frame_rate ());
				_just_sought = false;
			}

			double const next = _video_position / _ffmpeg_content->video_frame_rate();
			double const one_frame = 1 / _ffmpeg_content->video_frame_rate ();
			double delta = pts - next;

			while (delta > one_frame) {
				/* This PTS is more than one frame forward in time of where we think we should be; emit
				   a black frame.
				*/

				/* XXX: I think this should be a copy of the last frame... */
				boost::shared_ptr<Image> black (
					new Image (
						static_cast<AVPixelFormat> (_frame->format),
						libdcp::Size (video_codec_context()->width, video_codec_context()->height),
						true
						)
					);
				
				black->make_black ();
				video (image, false, _video_position);
				delta -= one_frame;
			}

			if (delta > -one_frame) {
				/* This PTS is within a frame of being right; emit this (otherwise it will be dropped) */
				video (image, false, _video_position);
			}
				
		} else {
			shared_ptr<const Film> film = _film.lock ();
			assert (film);
			film->log()->log ("Dropping frame without PTS");
		}
	}

	return true;
}

	
void
FFmpegDecoder::setup_subtitle ()
{
	boost::mutex::scoped_lock lm (_mutex);
	
	if (!_ffmpeg_content->subtitle_stream() || _ffmpeg_content->subtitle_stream()->index (_format_context) >= int (_format_context->nb_streams)) {
		return;
	}

	_subtitle_codec_context = _ffmpeg_content->subtitle_stream()->stream(_format_context)->codec;
	_subtitle_codec = avcodec_find_decoder (_subtitle_codec_context->codec_id);

	if (_subtitle_codec == 0) {
		throw DecodeError (_("could not find subtitle decoder"));
	}
	
	if (avcodec_open2 (_subtitle_codec_context, _subtitle_codec, 0) < 0) {
		throw DecodeError (N_("could not open subtitle decoder"));
	}
}

bool
FFmpegDecoder::done () const
{
	bool const vd = !_decode_video || (_video_position >= _ffmpeg_content->video_length());
	bool const ad = !_decode_audio || !_ffmpeg_content->audio_stream() || (_audio_position >= _ffmpeg_content->audio_length());
	return vd && ad;
}
	
void
FFmpegDecoder::decode_subtitle_packet ()
{
	int got_subtitle;
	AVSubtitle sub;
	if (avcodec_decode_subtitle2 (_subtitle_codec_context, &sub, &got_subtitle, &_packet) < 0 || !got_subtitle) {
		return;
	}

	/* Sometimes we get an empty AVSubtitle, which is used by some codecs to
	   indicate that the previous subtitle should stop.
	*/
	if (sub.num_rects <= 0) {
		subtitle (shared_ptr<Image> (), dcpomatic::Rect<double> (), 0, 0);
		return;
	} else if (sub.num_rects > 1) {
		throw DecodeError (_("multi-part subtitles not yet supported"));
	}
		
	/* Subtitle PTS in seconds (within the source, not taking into account any of the
	   source that we may have chopped off for the DCP)
	*/
	double const packet_time = (static_cast<double> (sub.pts ) / AV_TIME_BASE) + _pts_offset;

	/* hence start time for this sub */
	Time const from = (packet_time + (double (sub.start_display_time) / 1e3)) * TIME_HZ;
	Time const to = (packet_time + (double (sub.end_display_time) / 1e3)) * TIME_HZ;

	AVSubtitleRect const * rect = sub.rects[0];

	if (rect->type != SUBTITLE_BITMAP) {
		throw DecodeError (_("non-bitmap subtitles not yet supported"));
	}

	/* Note RGBA is expressed little-endian, so the first byte in the word is R, second
	   G, third B, fourth A.
	*/
	shared_ptr<Image> image (new Image (PIX_FMT_RGBA, libdcp::Size (rect->w, rect->h), true));

	/* Start of the first line in the subtitle */
	uint8_t* sub_p = rect->pict.data[0];
	/* sub_p looks up into a BGRA palette which is here
	   (i.e. first byte B, second G, third R, fourth A)
	*/
	uint32_t const * palette = (uint32_t *) rect->pict.data[1];
	/* Start of the output data */
	uint32_t* out_p = (uint32_t *) image->data()[0];

	for (int y = 0; y < rect->h; ++y) {
		uint8_t* sub_line_p = sub_p;
		uint32_t* out_line_p = out_p;
		for (int x = 0; x < rect->w; ++x) {
			uint32_t const p = palette[*sub_line_p++];
			*out_line_p++ = ((p & 0xff) << 16) | (p & 0xff00) | ((p & 0xff0000) >> 16) | (p & 0xff000000);
		}
		sub_p += rect->pict.linesize[0];
		out_p += image->stride()[0] / sizeof (uint32_t);
	}

	libdcp::Size const vs = _ffmpeg_content->video_size ();

	subtitle (
		image,
		dcpomatic::Rect<double> (
			static_cast<double> (rect->x) / vs.width,
			static_cast<double> (rect->y) / vs.height,
			static_cast<double> (rect->w) / vs.width,
			static_cast<double> (rect->h) / vs.height
			),
		from,
		to
		);
			  
	
	avsubtitle_free (&sub);
}
