/*
    Copyright (C) 2017 Carl Hetherington <cth@carlh.net>

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

#include "transcoder.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

class FFmpegTranscoder : public Transcoder
{
public:
	FFmpegTranscoder (boost::shared_ptr<const Film> film, boost::weak_ptr<Job> job);

	void go ();

	float current_encoding_rate () const;
	int video_frames_enqueued () const;
	bool finishing () const {
		return false;
	}

private:
	void video (boost::shared_ptr<PlayerVideo>, DCPTime);
	void audio (boost::shared_ptr<AudioBuffers>, DCPTime);
	void subtitle (PlayerSubtitles, DCPTimePeriod);

	AVCodecContext* _codec_context;
	AVFormatContext* _format_context;
	AVStream* _video_stream;
	AVPixelFormat _pixel_format;
};
