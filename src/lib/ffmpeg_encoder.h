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

#ifndef DCPOMATIC_FFMPEG_ENCODER_H
#define DCPOMATIC_FFMPEG_ENCODER_H

#include "encoder.h"
#include "event_history.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

class FFmpegEncoder : public Encoder
{
public:
	enum Format
	{
		FORMAT_PRORES,
		FORMAT_H264
	};

	FFmpegEncoder (boost::shared_ptr<const Film> film, boost::weak_ptr<Job> job, boost::filesystem::path output, Format format);

	void go ();

	float current_rate () const;
	Frame frames_done () const;
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
	AVDictionary* _options;
	std::string _codec_name;

	mutable boost::mutex _mutex;
	DCPTime _last_time;

	EventHistory _history;

	boost::filesystem::path _output;
};

#endif
