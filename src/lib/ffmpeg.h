/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef DCPOMATIC_FFMPEG_H
#define DCPOMATIC_FFMPEG_H

#include "file_group.h"
#include "ffmpeg_subtitle_period.h"
extern "C" {
#include <libavcodec/avcodec.h>
}
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>

struct AVFormatContext;
struct AVFrame;
struct AVIOContext;

class FFmpegContent;
class FFmpegAudioStream;
class Log;

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
	AVCodecContext* subtitle_codec_context () const;
	ContentTime pts_offset (
		std::vector<boost::shared_ptr<FFmpegAudioStream> > audio_streams, boost::optional<ContentTime> first_video, double video_frame_rate
		) const;

	static FFmpegSubtitlePeriod subtitle_period (AVSubtitle const & sub);
	static std::string subtitle_id (AVSubtitle const & sub);
	static bool subtitle_starts_image (AVSubtitle const & sub);

	boost::shared_ptr<const FFmpegContent> _ffmpeg_content;

	uint8_t* _avio_buffer;
	int _avio_buffer_size;
	AVIOContext* _avio_context;
	FileGroup _file_group;

	AVFormatContext* _format_context;
	AVPacket _packet;
	AVFrame* _frame;

	/** Index of video stream within AVFormatContext */
	boost::optional<int> _video_stream;

	/* It would appear (though not completely verified) that one must have
	   a mutex around calls to avcodec_open* and avcodec_close... and here
	   it is.
	*/
	static boost::mutex _mutex;

private:
	void setup_general ();
	void setup_decoders ();

	static void ffmpeg_log_callback (void* ptr, int level, const char* fmt, va_list vl);
	static boost::weak_ptr<Log> _ffmpeg_log;
};

#endif
