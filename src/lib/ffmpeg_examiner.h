/*
    Copyright (C) 2013-2017 Carl Hetherington <cth@carlh.net>

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


#include "ffmpeg.h"
#include "video_examiner.h"
#include <boost/optional.hpp>


struct AVStream;
class FFmpegAudioStream;
class FFmpegSubtitleStream;
class Job;


class FFmpegExaminer : public FFmpeg, public VideoExaminer
{
public:
	FFmpegExaminer (std::shared_ptr<const FFmpegContent>, std::shared_ptr<Job> job = std::shared_ptr<Job>());

	bool has_video () const override;

	boost::optional<double> video_frame_rate () const override;
	boost::optional<dcp::Size> video_size() const override;
	Frame video_length () const override;
	boost::optional<double> sample_aspect_ratio () const override;
	bool yuv () const override;

	std::vector<std::shared_ptr<FFmpegSubtitleStream>> subtitle_streams () const {
		return _subtitle_streams;
	}

	std::vector<std::shared_ptr<FFmpegAudioStream>> audio_streams () const {
		return _audio_streams;
	}

	boost::optional<dcpomatic::ContentTime> first_video () const {
		return _first_video;
	}

	VideoRange range () const override;

	PixelQuanta pixel_quanta () const override;

	AVColorRange color_range () const {
		return video_codec_context()->color_range;
	}

	AVColorPrimaries color_primaries () const {
		return video_codec_context()->color_primaries;
	}

	AVColorTransferCharacteristic color_trc () const {
		return video_codec_context()->color_trc;
	}

	AVColorSpace colorspace () const {
		return video_codec_context()->colorspace;
	}

	boost::optional<int> bits_per_pixel () const;

	bool has_alpha() const;

	boost::optional<double> rotation () const {
		return _rotation;
	}

	bool pulldown () const {
		return _pulldown;
	}

private:
	bool video_packet (AVCodecContext* context, std::string& temporal_reference, AVPacket* packet);
	bool audio_packet (AVCodecContext* context, std::shared_ptr<FFmpegAudioStream>, AVPacket* packet);

	std::string stream_name (AVStream* s) const;
	std::string subtitle_stream_name (AVStream* s) const;
	boost::optional<dcpomatic::ContentTime> frame_time (AVFrame* frame, AVStream* stream) const;

	std::vector<std::shared_ptr<FFmpegSubtitleStream>> _subtitle_streams;
	std::vector<std::shared_ptr<FFmpegAudioStream>> _audio_streams;
	boost::optional<dcpomatic::ContentTime> _first_video;
	/** Video length, either obtained from the header or derived by running
	 *  through the whole file.
	 */
	Frame _video_length = 0;
	bool _need_length = false;

	boost::optional<double> _rotation;
	bool _pulldown = false;

	struct SubtitleStart
	{
		SubtitleStart (std::string id_, bool image_, dcpomatic::ContentTime time_)
			: id (id_)
			, image (image_)
			, time (time_)
		{}

		std::string id;
		/** true if it's an image subtitle, false for text */
		bool image;
		dcpomatic::ContentTime time;
	};

	typedef std::map<std::shared_ptr<FFmpegSubtitleStream>, boost::optional<SubtitleStart>> LastSubtitleMap;
	LastSubtitleMap _last_subtitle_start;
};
