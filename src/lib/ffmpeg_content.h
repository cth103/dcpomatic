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


#ifndef DCPOMATIC_FFMPEG_CONTENT_H
#define DCPOMATIC_FFMPEG_CONTENT_H


#include "audio_stream.h"
#include "content.h"
#include "filter.h"


struct AVFormatContext;
struct AVStream;


class FFmpegAudioStream;
class FFmpegSubtitleStream;
class Filter;
class VideoContent;
struct ffmpeg_pts_offset_test;
struct audio_sampling_rate_test;


class FFmpegContentProperty
{
public:
	static int constexpr SUBTITLE_STREAMS = 100;
	/** The chosen subtitle stream, or something about it */
	static int constexpr SUBTITLE_STREAM = 101;
	static int constexpr FILTERS = 102;
	static int constexpr KDM = 103;
};


class FFmpegContent : public Content
{
public:
	FFmpegContent(boost::filesystem::path);
	FFmpegContent(cxml::ConstNodePtr, boost::optional<boost::filesystem::path> film_directory, int version, std::list<std::string> &);
	FFmpegContent(std::vector<std::shared_ptr<Content>>);

	std::shared_ptr<FFmpegContent> shared_from_this() {
		return std::dynamic_pointer_cast<FFmpegContent>(Content::shared_from_this());
	}

	std::shared_ptr<const FFmpegContent> shared_from_this() const {
		return std::dynamic_pointer_cast<const FFmpegContent>(Content::shared_from_this());
	}

	void examine(std::shared_ptr<const Film> film, std::shared_ptr<Job>, bool tolerant) override;
	void take_settings_from(std::shared_ptr<const Content> c) override;
	std::string summary() const override;
	std::string technical_summary() const override;

	void as_xml(
		xmlpp::Element* element,
		bool with_paths,
		PathBehaviour path_behaviour,
		boost::optional<boost::filesystem::path> film_directory
		) const override;

	dcpomatic::DCPTime full_length(std::shared_ptr<const Film> film) const override;
	dcpomatic::DCPTime approximate_length() const override;

	std::string identifier() const override;

	void set_default_colour_conversion();

	void set_filters(std::vector<Filter> const&);

	std::vector<std::shared_ptr<FFmpegSubtitleStream>> subtitle_streams() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _subtitle_streams;
	}

	std::shared_ptr<FFmpegSubtitleStream> subtitle_stream() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _subtitle_stream;
	}

	std::vector<std::shared_ptr<FFmpegAudioStream>> ffmpeg_audio_streams() const;

	std::vector<Filter> filters() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _filters;
	}

	void set_subtitle_stream(std::shared_ptr<FFmpegSubtitleStream>);

	boost::optional<dcpomatic::ContentTime> first_video() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _first_video;
	}

	void signal_subtitle_stream_changed();

private:
	void add_properties(std::shared_ptr<const Film> film, std::list<UserProperty> &) const override;

	friend struct ffmpeg_pts_offset_test;
	friend struct audio_sampling_rate_test;

	std::vector<std::shared_ptr<FFmpegSubtitleStream>> _subtitle_streams;
	std::shared_ptr<FFmpegSubtitleStream> _subtitle_stream;
	boost::optional<dcpomatic::ContentTime> _first_video;
	/** Video filters that should be used when generating DCPs */
	std::vector<Filter> _filters;

	boost::optional<AVColorRange> _color_range;
	boost::optional<AVColorPrimaries> _color_primaries;
	boost::optional<AVColorTransferCharacteristic> _color_trc;
	boost::optional<AVColorSpace> _colorspace;
	boost::optional<int> _bits_per_pixel;
};

#endif
