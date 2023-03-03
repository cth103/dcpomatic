/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
extern "C" {
#include <libavcodec/avcodec.h>
}
LIBDCP_ENABLE_WARNINGS
#include <boost/thread/mutex.hpp>


struct AVFormatContext;
struct AVFrame;
struct AVStream;
struct AVIOContext;

class FFmpegContent;
class FFmpegAudioStream;
class Log;


class FFmpeg
{
public:
	explicit FFmpeg (std::shared_ptr<const FFmpegContent>);
	virtual ~FFmpeg ();

	std::shared_ptr<const FFmpegContent> ffmpeg_content () const {
		return _ffmpeg_content;
	}

	int avio_read (uint8_t *, int);
	int64_t avio_seek (int64_t, int);

protected:
	AVCodecContext* video_codec_context () const;
	AVCodecContext* subtitle_codec_context () const;
	dcpomatic::ContentTime pts_offset (
		std::vector<std::shared_ptr<FFmpegAudioStream>> audio_streams, boost::optional<dcpomatic::ContentTime> first_video, double video_frame_rate
		) const;

	static FFmpegSubtitlePeriod subtitle_period (AVPacket const* packet, AVStream const* stream, AVSubtitle const & sub);

	std::shared_ptr<const FFmpegContent> _ffmpeg_content;

	uint8_t* _avio_buffer = nullptr;
	int _avio_buffer_size = 4096;
	AVIOContext* _avio_context = nullptr;
	FileGroup _file_group;

	AVFormatContext* _format_context = nullptr;
	std::vector<AVCodecContext*> _codec_context;

	/** AVFrame used for decoding video */
	AVFrame* _video_frame = nullptr;
	/** Index of video stream within AVFormatContext */
	boost::optional<int> _video_stream;

	AVFrame* audio_frame (std::shared_ptr<const FFmpegAudioStream> stream);

	/* It would appear (though not completely verified) that one must have
	   a mutex around calls to avcodec_open* and avcodec_close... and here
	   it is.
	*/
	static boost::mutex _mutex;

private:
	void setup_general ();
	void setup_decoders ();

	static void ffmpeg_log_callback (void* ptr, int level, const char* fmt, va_list vl);

	/** AVFrames used for decoding audio streams; accessed with audio_frame() */
	std::map<std::shared_ptr<const FFmpegAudioStream>, AVFrame*> _audio_frame;
};


#endif
