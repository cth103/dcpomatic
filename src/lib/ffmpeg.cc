/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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
}
#include "ffmpeg.h"
#include "ffmpeg_content.h"
#include "ffmpeg_audio_stream.h"
#include "ffmpeg_subtitle_stream.h"
#include "exceptions.h"
#include "util.h"
#include "raw_convert.h"

#include "i18n.h"

using std::string;
using std::cout;
using boost::shared_ptr;

boost::mutex FFmpeg::_mutex;

FFmpeg::FFmpeg (boost::shared_ptr<const FFmpegContent> c)
	: _ffmpeg_content (c)
	, _avio_buffer (0)
	, _avio_buffer_size (4096)
	, _avio_context (0)
	, _format_context (0)
	, _frame (0)
	, _video_stream (-1)
{
	setup_general ();
	setup_decoders ();
}

FFmpeg::~FFmpeg ()
{
	boost::mutex::scoped_lock lm (_mutex);

	for (uint32_t i = 0; i < _format_context->nb_streams; ++i) {
		avcodec_close (_format_context->streams[i]->codec);
	}

	av_frame_free (&_frame);
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
FFmpeg::setup_general ()
{
	av_register_all ();

	_file_group.set_paths (_ffmpeg_content->paths ());
	_avio_buffer = static_cast<uint8_t*> (wrapped_av_malloc (_avio_buffer_size));
	_avio_context = avio_alloc_context (_avio_buffer, _avio_buffer_size, 0, this, avio_read_wrapper, 0, avio_seek_wrapper);
	_format_context = avformat_alloc_context ();
	_format_context->pb = _avio_context;
	
	AVDictionary* options = 0;
	/* These durations are in microseconds, and represent how far into the content file
	   we will look for streams.
	*/
	av_dict_set (&options, "analyzeduration", raw_convert<string> (5 * 60 * 1000000).c_str(), 0);
	av_dict_set (&options, "probesize", raw_convert<string> (5 * 60 * 1000000).c_str(), 0);
	
	if (avformat_open_input (&_format_context, 0, 0, &options) < 0) {
		throw OpenFileError (_ffmpeg_content->path(0).string ());
	}

	if (avformat_find_stream_info (_format_context, 0) < 0) {
		throw DecodeError (_("could not find stream information"));
	}

	/* Find video stream */

	int video_stream_undefined_frame_rate = -1;

	for (uint32_t i = 0; i < _format_context->nb_streams; ++i) {
		AVStream* s = _format_context->streams[i];
		/* Files from iTunes sometimes have two video streams, one with the avg_frame_rate.num and .den set
		   to zero.  Ignore these streams.
		*/
		if (s->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			if (s->avg_frame_rate.num > 0 && s->avg_frame_rate.den > 0) {
				/* This is definitely our video stream */
				_video_stream = i;
			} else {
				/* This is our video stream if we don't get a better offer */
				video_stream_undefined_frame_rate = i;
			}
		}
	}

	/* Files from iTunes sometimes have two video streams, one with the avg_frame_rate.num and .den set
	   to zero.  Only use such a stream if there is no alternative.
	*/
	if (_video_stream == -1 && video_stream_undefined_frame_rate != -1) {
		_video_stream = video_stream_undefined_frame_rate;
	}	
	
	if (_video_stream < 0) {
		throw DecodeError (N_("could not find video stream"));
	}

	/* Hack: if the AVStreams have zero IDs, put some in.  We
	   use the IDs so that we can cope with VOBs, in which streams
	   move about in index but remain with the same ID in different
	   VOBs.  However, some files have all-zero IDs, hence this hack.
	*/
	   
	uint32_t i = 0;
	while (i < _format_context->nb_streams && _format_context->streams[i]->id == 0) {
		++i;
	}

	if (i == _format_context->nb_streams) {
		/* Put in our own IDs */
		for (uint32_t i = 0; i < _format_context->nb_streams; ++i) {
			_format_context->streams[i]->id = i;
		}
	}

	_frame = av_frame_alloc ();
	if (_frame == 0) {
		throw DecodeError (N_("could not allocate frame"));
	}
}

void
FFmpeg::setup_decoders ()
{
	boost::mutex::scoped_lock lm (_mutex);

	for (uint32_t i = 0; i < _format_context->nb_streams; ++i) {
		AVCodecContext* context = _format_context->streams[i]->codec;
		
		AVCodec* codec = avcodec_find_decoder (context->codec_id);
		if (codec) {

			/* This option disables decoding of DCA frame footers in our patched version
			   of FFmpeg.  I believe these footers are of no use to us, and they can cause
			   problems when FFmpeg fails to decode them (mantis #352).
			*/
			AVDictionary* options = 0;
			av_dict_set (&options, "disable_footer", "1", 0);
			
			if (avcodec_open2 (context, codec, &options) < 0) {
				throw DecodeError (N_("could not open decoder"));
			}
		}

		/* We are silently ignoring any failures to find suitable decoders here */
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
	if (!_ffmpeg_content->audio_stream ()) {
		return 0;
	}
	
	return _ffmpeg_content->audio_stream()->stream(_format_context)->codec;
}

AVCodecContext *
FFmpeg::subtitle_codec_context () const
{
	if (!_ffmpeg_content->subtitle_stream ()) {
		return 0;
	}
	
	return _ffmpeg_content->subtitle_stream()->stream(_format_context)->codec;
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
