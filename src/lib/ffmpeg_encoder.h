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
#include "audio_mapping.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include <boost/thread/condition.hpp>

class Butler;

class FFmpegEncoder : public Encoder
{
public:
	enum Format
	{
		FORMAT_PRORES,
		FORMAT_H264
	};

	FFmpegEncoder (boost::shared_ptr<const Film> film, boost::weak_ptr<Job> job, boost::filesystem::path output, Format format, bool mixdown_to_stereo);

	void go ();

	float current_rate () const;
	Frame frames_done () const;
	bool finishing () const {
		return false;
	}

private:
	void video (boost::shared_ptr<PlayerVideo>, DCPTime);
	void audio (boost::shared_ptr<AudioBuffers>);
	void subtitle (PlayerText, DCPTimePeriod);

	void setup_video ();
	void setup_audio ();

	void audio_frame (int size);

	static void buffer_free(void* opaque, uint8_t* data);
	void buffer_free2(uint8_t* data);

	AVCodec* _video_codec;
	AVCodecContext* _video_codec_context;
	AVCodec* _audio_codec;
	AVCodecContext* _audio_codec_context;
	AVFormatContext* _format_context;
	AVStream* _video_stream;
	AVStream* _audio_stream;
	AVPixelFormat _pixel_format;
	AVSampleFormat _sample_format;
	AVDictionary* _video_options;
	std::string _video_codec_name;
	std::string _audio_codec_name;
	int _output_audio_channels;

	mutable boost::mutex _mutex;
	DCPTime _last_time;

	EventHistory _history;

	boost::filesystem::path _output;

	boost::shared_ptr<AudioBuffers> _pending_audio;

	mutable boost::mutex _queue_mutex;
	boost::condition _queue_full;
	std::list<std::pair<boost::shared_ptr<PlayerVideo>, DCPTime> > _queue;

	boost::shared_ptr<Butler> _butler;

	/** Store of shared_ptr<Image> to keep them alive whilst raw pointers into
	    their data have been passed to FFmpeg.
	*/
	std::map<uint8_t*, boost::shared_ptr<const Image> > _pending_images;

	static int _video_stream_index;
	static int _audio_stream_index;
};

#endif
