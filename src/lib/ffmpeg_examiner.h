/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#include "ffmpeg.h"
#include "video_examiner.h"
#include <boost/optional.hpp>

class FFmpegAudioStream;
class FFmpegSubtitleStream;

class FFmpegExaminer : public FFmpeg, public VideoExaminer
{
public:
	FFmpegExaminer (boost::shared_ptr<const FFmpegContent>, boost::shared_ptr<Job> job = boost::shared_ptr<Job> ());
	
	boost::optional<float> video_frame_rate () const;
	dcp::Size video_size () const;
	ContentTime video_length () const;
	boost::optional<float> sample_aspect_ratio () const;

	std::vector<boost::shared_ptr<FFmpegSubtitleStream> > subtitle_streams () const {
		return _subtitle_streams;
	}
	
	std::vector<boost::shared_ptr<FFmpegAudioStream> > audio_streams () const {
		return _audio_streams;
	}

	boost::optional<ContentTime> first_video () const {
		return _first_video;
	}
	
private:
	void video_packet (AVCodecContext *);
	void audio_packet (AVCodecContext *, boost::shared_ptr<FFmpegAudioStream>);
	void subtitle_packet (AVCodecContext *, boost::shared_ptr<FFmpegSubtitleStream>);
	
	std::string stream_name (AVStream* s) const;
	std::string audio_stream_name (AVStream* s) const;
	std::string subtitle_stream_name (AVStream* s) const;
	boost::optional<ContentTime> frame_time (AVStream* s) const;
	
	std::vector<boost::shared_ptr<FFmpegSubtitleStream> > _subtitle_streams;
	std::vector<boost::shared_ptr<FFmpegAudioStream> > _audio_streams;
	boost::optional<ContentTime> _first_video;
	/** Video length, either obtained from the header or derived by running
	 *  through the whole file.
	 */
	ContentTime _video_length;
	bool _need_video_length;
};
