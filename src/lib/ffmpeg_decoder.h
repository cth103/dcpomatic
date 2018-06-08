/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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

/** @file  src/ffmpeg_decoder.h
 *  @brief A decoder using FFmpeg to decode content.
 */

#include "util.h"
#include "decoder.h"
#include "ffmpeg.h"
extern "C" {
#include <libavcodec/avcodec.h>
}
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <stdint.h>

class Log;
class VideoFilterGraph;
class FFmpegAudioStream;
class AudioBuffers;
class Image;
struct ffmpeg_pts_offset_test;

/** @class FFmpegDecoder
 *  @brief A decoder using FFmpeg to decode content.
 */
class FFmpegDecoder : public FFmpeg, public Decoder
{
public:
	FFmpegDecoder (boost::shared_ptr<const FFmpegContent>, boost::shared_ptr<Log> log, bool fast);

	bool pass ();
	void seek (ContentTime time, bool);

private:
	friend struct ::ffmpeg_pts_offset_test;

	void flush ();

	AVSampleFormat audio_sample_format (boost::shared_ptr<FFmpegAudioStream> stream) const;
	int bytes_per_audio_sample (boost::shared_ptr<FFmpegAudioStream> stream) const;

	bool decode_video_packet ();
	void decode_audio_packet ();
	void decode_subtitle_packet ();

	void decode_bitmap_subtitle (AVSubtitleRect const * rect, ContentTime from);
	void decode_ass_subtitle (std::string ass, ContentTime from);

	void maybe_add_subtitle ();
	boost::shared_ptr<AudioBuffers> deinterleave_audio (boost::shared_ptr<FFmpegAudioStream> stream) const;

	boost::shared_ptr<Log> _log;

	std::list<boost::shared_ptr<VideoFilterGraph> > _filter_graphs;
	boost::mutex _filter_graphs_mutex;

	ContentTime _pts_offset;
	boost::optional<ContentTime> _current_subtitle_to;
	/** true if we have a subtitle which has not had emit_stop called for it yet */
	bool _have_current_subtitle;

	boost::shared_ptr<Image> _black_image;

	std::vector<boost::optional<ContentTime> > _next_time;
};
