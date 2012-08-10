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
#include "film_state.h"
#include "options.h"
#include "exceptions.h"
#include "image.h"
#include "util.h"
#include "log.h"
#include "ffmpeg_decoder.h"

using namespace std;
using namespace boost;

FFmpegDecoder::FFmpegDecoder (boost::shared_ptr<const FilmState> s, boost::shared_ptr<const Options> o, Job* j, Log* l, bool minimal, bool ignore_length)
	: Decoder (s, o, j, l, minimal, ignore_length)
	, _format_context (0)
	, _video_stream (-1)
	, _audio_stream (-1)
	, _frame (0)
	, _video_codec_context (0)
	, _video_codec (0)
	, _audio_codec_context (0)
	, _audio_codec (0)
{
	setup_general ();
	setup_video ();
	setup_audio ();
}

FFmpegDecoder::~FFmpegDecoder ()
{
	if (_audio_codec_context) {
		avcodec_close (_audio_codec_context);
	}
	
	if (_video_codec_context) {
		avcodec_close (_video_codec_context);
	}
	
	av_free (_frame);
	avformat_close_input (&_format_context);
}	

void
FFmpegDecoder::setup_general ()
{
	int r;
	
	av_register_all ();

	if ((r = avformat_open_input (&_format_context, _fs->content_path().c_str(), 0, 0)) != 0) {
		throw OpenFileError (_fs->content_path ());
	}

	if (avformat_find_stream_info (_format_context, 0) < 0) {
		throw DecodeError ("could not find stream information");
	}

	for (uint32_t i = 0; i < _format_context->nb_streams; ++i) {
		if (_format_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			_video_stream = i;
		} else if (_format_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			_audio_stream = i;
		}
	}

	if (_video_stream < 0) {
		throw DecodeError ("could not find video stream");
	}

	_frame = avcodec_alloc_frame ();
	if (_frame == 0) {
		throw DecodeError ("could not allocate frame");
	}
}

void
FFmpegDecoder::setup_video ()
{
	_video_codec_context = _format_context->streams[_video_stream]->codec;
	_video_codec = avcodec_find_decoder (_video_codec_context->codec_id);

	if (_video_codec == 0) {
		throw DecodeError ("could not find video decoder");
	}
	
	if (avcodec_open2 (_video_codec_context, _video_codec, 0) < 0) {
		throw DecodeError ("could not open video decoder");
	}
}

void
FFmpegDecoder::setup_audio ()
{
	if (_audio_stream < 0) {
		return;
	}
	
	_audio_codec_context = _format_context->streams[_audio_stream]->codec;
	_audio_codec = avcodec_find_decoder (_audio_codec_context->codec_id);

	if (_audio_codec == 0) {
		throw DecodeError ("could not find audio decoder");
	}
	
	if (avcodec_open2 (_audio_codec_context, _audio_codec, 0) < 0) {
		throw DecodeError ("could not open audio decoder");
	}
}

bool
FFmpegDecoder::do_pass ()
{
	int r = av_read_frame (_format_context, &_packet);
	if (r < 0) {
		return true;
	}

	if (_packet.stream_index == _video_stream && _opt->decode_video) {
		
		int frame_finished;
		if (avcodec_decode_video2 (_video_codec_context, _frame, &frame_finished, &_packet) >= 0 && frame_finished) {
			process_video (_frame);
		}

	} else if (_audio_stream >= 0 && _packet.stream_index == _audio_stream && _opt->decode_audio) {
		
		avcodec_get_frame_defaults (_frame);
		
		int frame_finished;
		if (avcodec_decode_audio4 (_audio_codec_context, _frame, &frame_finished, &_packet) >= 0 && frame_finished) {
			int const data_size = av_samples_get_buffer_size (
				0, _audio_codec_context->channels, _frame->nb_samples, audio_sample_format (), 1
				);

			assert (_audio_codec_context->channels == _fs->audio_channels);
			process_audio (_frame->data[0], data_size);
		}
	}
	
	av_free_packet (&_packet);
	return false;
}

int
FFmpegDecoder::length_in_frames () const
{
	return (_format_context->duration / AV_TIME_BASE) * frames_per_second ();
}

float
FFmpegDecoder::frames_per_second () const
{
	return av_q2d (_format_context->streams[_video_stream]->avg_frame_rate);
}

int
FFmpegDecoder::audio_channels () const
{
	if (_audio_codec_context == 0) {
		return 0;
	}
	
	return _audio_codec_context->channels;
}

int
FFmpegDecoder::audio_sample_rate () const
{
	if (_audio_codec_context == 0) {
		return 0;
	}
	
	return _audio_codec_context->sample_rate;
}

AVSampleFormat
FFmpegDecoder::audio_sample_format () const
{
	if (_audio_codec_context == 0) {
		return (AVSampleFormat) 0;
	}
	
	return _audio_codec_context->sample_fmt;
}

int64_t
FFmpegDecoder::audio_channel_layout () const
{
	if (_audio_codec_context == 0) {
		return 0;
	}
	
	return _audio_codec_context->channel_layout;
}

Size
FFmpegDecoder::native_size () const
{
	return Size (_video_codec_context->width, _video_codec_context->height);
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

