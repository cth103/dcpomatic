/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libpostproc/postprocess.h>
}
#include "ffmpeg.h"
#include "ffmpeg_content.h"
#include "exceptions.h"

#include "i18n.h"

using std::string;
using std::cout;
using std::stringstream;
using boost::shared_ptr;

boost::mutex FFmpeg::_mutex;

FFmpeg::FFmpeg (boost::shared_ptr<const FFmpegContent> c)
	: _ffmpeg_content (c)
	, _format_context (0)
	, _frame (0)
	, _video_stream (-1)
{
	setup_general ();
	setup_video ();
	setup_audio ();
}

FFmpeg::~FFmpeg ()
{
	boost::mutex::scoped_lock lm (_mutex);

	for (uint32_t i = 0; i < _format_context->nb_streams; ++i) {
		AVCodecContext* context = _format_context->streams[i]->codec;
		if (context->codec_type == AVMEDIA_TYPE_VIDEO || context->codec_type == AVMEDIA_TYPE_AUDIO) {
			avcodec_close (context);
		}
	}

	av_free (_frame);
	
	avformat_close_input (&_format_context);
}

void
FFmpeg::setup_general ()
{
	av_register_all ();

	if (avformat_open_input (&_format_context, _ffmpeg_content->path().string().c_str(), 0, 0) < 0) {
		throw OpenFileError (_ffmpeg_content->path().string ());
	}

	if (avformat_find_stream_info (_format_context, 0) < 0) {
		throw DecodeError (_("could not find stream information"));
	}

	/* Find video stream */

	for (uint32_t i = 0; i < _format_context->nb_streams; ++i) {
		AVStream* s = _format_context->streams[i];
		if (s->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			_video_stream = i;
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
FFmpeg::setup_video ()
{
	boost::mutex::scoped_lock lm (_mutex);
	
	AVCodecContext* context = _format_context->streams[_video_stream]->codec;
	AVCodec* codec = avcodec_find_decoder (context->codec_id);

	if (codec == 0) {
		throw DecodeError (_("could not find video decoder"));
	}

	if (avcodec_open2 (context, codec, 0) < 0) {
		throw DecodeError (N_("could not open video decoder"));
	}
}

void
FFmpeg::setup_audio ()
{
	boost::mutex::scoped_lock lm (_mutex);

	for (uint32_t i = 0; i < _format_context->nb_streams; ++i) {
		AVCodecContext* context = _format_context->streams[i]->codec;
		if (context->codec_type != AVMEDIA_TYPE_AUDIO) {
			continue;
		}
		
		AVCodec* codec = avcodec_find_decoder (context->codec_id);
		if (codec == 0) {
			throw DecodeError (_("could not find audio decoder"));
		}
		
		if (avcodec_open2 (context, codec, 0) < 0) {
			throw DecodeError (N_("could not open audio decoder"));
		}
	}
}


AVCodecContext *
FFmpeg::video_codec_context () const
{
	return _format_context->streams[_video_stream]->codec;
}

AVCodecContext *
FFmpeg::audio_codec_context () const
{
	return _format_context->streams[_ffmpeg_content->audio_stream()->id]->codec;
}
