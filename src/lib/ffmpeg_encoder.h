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

#ifndef DCPOMATIC_FFMPEG_ENCODER_H
#define DCPOMATIC_FFMPEG_ENCODER_H

#include "encoder.h"
#include "event_history.h"
#include "audio_mapping.h"
#include "ffmpeg_file_encoder.h"

class Butler;

class FFmpegEncoder : public Encoder
{
public:
	FFmpegEncoder (
		std::shared_ptr<const Film> film,
		std::weak_ptr<Job> job,
		boost::filesystem::path output,
		ExportFormat format,
		bool mixdown_to_stereo,
		bool split_reels,
		bool audio_stream_per_channel,
		int x264_crf
		);

	void go () override;

	boost::optional<float> current_rate () const override;
	Frame frames_done () const override;
	bool finishing () const override {
		return false;
	}

private:

	class FileEncoderSet
	{
	public:
		FileEncoderSet (
			dcp::Size video_frame_size,
			int video_frame_rate,
			int audio_frame_rate,
			int channels,
			ExportFormat,
			bool audio_stream_per_channel,
			int x264_crf,
			bool three_d,
			boost::filesystem::path output,
			std::string extension
			);

		std::shared_ptr<FFmpegFileEncoder> get (Eyes eyes) const;
		void flush ();
		void audio (std::shared_ptr<AudioBuffers>);

	private:
		std::map<Eyes, std::shared_ptr<FFmpegFileEncoder>> _encoders;
	};

	int _output_audio_channels;

	mutable boost::mutex _mutex;
	dcpomatic::DCPTime _last_time;

	EventHistory _history;

	boost::filesystem::path _output;
	ExportFormat _format;
	bool _split_reels;
	bool _audio_stream_per_channel;
	int _x264_crf;

	std::shared_ptr<Butler> _butler;
};

#endif
