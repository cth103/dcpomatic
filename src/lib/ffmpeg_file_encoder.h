/*
    Copyright (C) 2017-2018 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_FFMPEG_FILE_ENCODER_H
#define DCPOMATIC_FFMPEG_FILE_ENCODER_H

#include "encoder.h"
#include "event_history.h"
#include "audio_mapping.h"
#include "log.h"
#include <dcp/key.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}


class ExportAudioStream;


class FFmpegFileEncoder
{
public:
	FFmpegFileEncoder (
		dcp::Size video_frame_size,
		int video_frame_rate,
		int audio_frame_rate,
		int channels,
		ExportFormat,
		bool audio_stream_per_channel,
		int x264_crf,
		boost::filesystem::path output
		);

	~FFmpegFileEncoder ();

	void video (boost::shared_ptr<PlayerVideo>, dcpomatic::DCPTime);
	void audio (boost::shared_ptr<AudioBuffers>);
	void subtitle (PlayerText, dcpomatic::DCPTimePeriod);

	void flush ();

	static AVPixelFormat pixel_format (ExportFormat format);

private:
	void setup_video ();
	void setup_audio ();

	void audio_frame (int size);

	static void buffer_free(void* opaque, uint8_t* data);
	void buffer_free2(uint8_t* data);

	AVCodec* _video_codec;
	AVCodecContext* _video_codec_context;
	std::vector<boost::shared_ptr<ExportAudioStream> > _audio_streams;
	bool _audio_stream_per_channel;
	AVFormatContext* _format_context;
	AVStream* _video_stream;
	AVPixelFormat _pixel_format;
	AVSampleFormat _sample_format;
	AVDictionary* _video_options;
	std::string _video_codec_name;
	std::string _audio_codec_name;
	int _audio_channels;

	boost::filesystem::path _output;
	dcp::Size _video_frame_size;
	int _video_frame_rate;
	int _audio_frame_rate;

	int64_t _audio_frames;

	boost::shared_ptr<AudioBuffers> _pending_audio;

	/** Store of shared_ptr<Image> to keep them alive whilst raw pointers into
	    their data have been passed to FFmpeg.
	*/
	std::map<uint8_t*, boost::shared_ptr<const Image> > _pending_images;
	boost::mutex _pending_images_mutex;

	static int _video_stream_index;
	static int _audio_stream_index_base;
};

#endif
