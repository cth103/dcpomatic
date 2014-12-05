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

#ifndef DCPOMATIC_FFMPEG_H
#define DCPOMATIC_FFMPEG_H

extern "C" {
#include <libavcodec/avcodec.h>
}
#include "file_group.h"
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <vector>

struct AVFilterGraph;
struct AVCodecContext;
struct AVFilterContext;
struct AVFormatContext;
struct AVFrame;
struct AVBufferContext;
struct AVCodec;
struct AVStream;
struct AVIOContext;

class FFmpegContent;

class FFmpeg
{
public:
	FFmpeg (boost::shared_ptr<const FFmpegContent>);
	virtual ~FFmpeg ();

	boost::shared_ptr<const FFmpegContent> ffmpeg_content () const {
		return _ffmpeg_content;
	}

	int avio_read (uint8_t *, int);
	int64_t avio_seek (int64_t, int);

protected:
	AVCodecContext* video_codec_context () const;
	AVCodecContext* audio_codec_context () const;
	AVCodecContext* subtitle_codec_context () const;
	
	boost::shared_ptr<const FFmpegContent> _ffmpeg_content;

	uint8_t* _avio_buffer;
	int _avio_buffer_size;
	AVIOContext* _avio_context;
	FileGroup _file_group;
	
	AVFormatContext* _format_context;
	AVPacket _packet;
	AVFrame* _frame;

	/** Index of video stream within AVFormatContext */
	int _video_stream;

	/* It would appear (though not completely verified) that one must have
	   a mutex around calls to avcodec_open* and avcodec_close... and here
	   it is.
	*/
	static boost::mutex _mutex;

private:
	void setup_general ();
	void setup_decoders ();
};

#endif
