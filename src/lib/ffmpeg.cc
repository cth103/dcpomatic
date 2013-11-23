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
using boost::lexical_cast;

boost::mutex FFmpeg::_mutex;

/** @param long_probe true to do a long probe of the file looking for streams */
FFmpeg::FFmpeg (boost::shared_ptr<const FFmpegContent> c, bool long_probe)
	: _ffmpeg_content (c)
	, _avio_buffer (0)
	, _avio_buffer_size (4096)
	, _avio_context (0)
	, _format_context (0)
	, _frame (0)
	, _video_stream (-1)
{
	setup_general (long_probe);
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

	avcodec_free_frame (&_frame);
	
	avformat_close_input (&_format_context);
}

static int
avio_read_wrapper (void* data, uint8_t* buffer, int amount)
{
	return reinterpret_cast<FFmpeg*>(data)->avio_read (buffer, amount);
}

static int64_t
avio_seek_wrapper (void* data, int64_t offset, int whence)
{
	return reinterpret_cast<FFmpeg*>(data)->avio_seek (offset, whence);
}

void
FFmpeg::setup_general (bool long_probe)
{
	av_register_all ();

	_file_group.set_paths (_ffmpeg_content->paths ());
	_avio_buffer = static_cast<uint8_t*> (av_malloc (_avio_buffer_size));
	_avio_context = avio_alloc_context (_avio_buffer, _avio_buffer_size, 0, this, avio_read_wrapper, 0, avio_seek_wrapper);
	_format_context = avformat_alloc_context ();
	_format_context->pb = _avio_context;
	
	AVDictionary* options = 0;
	if (long_probe) {
		/* These durations are in microseconds, and represent how far into the content file
		   we will look for streams.
		*/
		av_dict_set (&options, "analyzeduration", lexical_cast<string> (5 * 60 * 1e6).c_str(), 0);
		av_dict_set (&options, "probesize", lexical_cast<string> (5 * 60 * 1e6).c_str(), 0);
	}
	
	if (avformat_open_input (&_format_context, 0, 0, &options) < 0) {
		throw OpenFileError (_ffmpeg_content->path(0).string ());
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

int
FFmpeg::avio_read (uint8_t* buffer, int const amount)
{
	return _file_group.read (buffer, amount);
}

int64_t
FFmpeg::avio_seek (int64_t const pos, int whence)
{
	if (whence == AVSEEK_SIZE) {
		return _file_group.length ();
	}
	
	return _file_group.seek (pos, whence);
}
