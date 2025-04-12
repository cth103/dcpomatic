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


#ifndef DCPOMATIC_VIDEO_CONTENT_H
#define DCPOMATIC_VIDEO_CONTENT_H


#include "colour_conversion.h"
#include "content_part.h"
#include "crop.h"
#include "dcpomatic_time.h"
#include "pixel_quanta.h"
#include "user_property.h"
#include "video_frame_type.h"
#include "video_range.h"
#include "types.h"
#include <dcp/language_tag.h>
#include <boost/thread/mutex.hpp>


class VideoExaminer;
class Ratio;
class Film;
class Content;


class VideoContentProperty
{
public:
	static int const USE;
	static int const SIZE;
	static int const FRAME_TYPE;
	static int const CROP;
	static int const COLOUR_CONVERSION;
	static int const FADE_IN;
	static int const FADE_OUT;
	static int const RANGE;
	static int const CUSTOM_RATIO;
	static int const CUSTOM_SIZE;
	static int const BURNT_SUBTITLE_LANGUAGE;
};


class VideoContent : public ContentPart, public std::enable_shared_from_this<VideoContent>
{
public:
	explicit VideoContent(Content* parent);
	VideoContent(Content* parent, cxml::ConstNodePtr node, int version, VideoRange video_range_hint);
	VideoContent(Content* parent, std::vector<std::shared_ptr<Content>>);

	void as_xml(xmlpp::Element*) const;
	std::string technical_summary() const;
	std::string identifier() const;
	void take_settings_from(std::shared_ptr<const VideoContent> c);

	Frame length() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _length;
	}

	Frame length_after_3d_combine() const {
		boost::mutex::scoped_lock lm(_mutex);
		if (_frame_type == VideoFrameType::THREE_D_ALTERNATE) {
			return _length / 2;
		}

		return _length;
	}

	boost::optional<dcp::Size> size() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _size;
	}

	void set_frame_type(VideoFrameType);

	void set_crop(Crop crop);
	void set_left_crop(int);
	void set_right_crop(int);
	void set_top_crop(int);
	void set_bottom_crop(int);

	void set_custom_ratio(boost::optional<float> ratio);
	void set_custom_size(boost::optional<dcp::Size> size);

	void unset_colour_conversion();
	void set_colour_conversion(ColourConversion);

	void set_fade_in(Frame);
	void set_fade_out(Frame);

	void set_range(VideoRange);
	void set_use(bool);

	void set_burnt_subtitle_language(boost::optional<dcp::LanguageTag> language);

	VideoFrameType frame_type() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _frame_type;
	}

	Crop actual_crop() const;

	Crop requested_crop() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _crop;
	}

	int requested_left_crop() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _crop.left;
	}

	int requested_right_crop() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _crop.right;
	}

	int requested_top_crop() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _crop.top;
	}

	int requested_bottom_crop() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _crop.bottom;
	}


	boost::optional<float> custom_ratio() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _custom_ratio;
	}


	boost::optional<dcp::Size> custom_size() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _custom_size;
	}


	boost::optional<ColourConversion> colour_conversion() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _colour_conversion;
	}

	boost::optional<double> sample_aspect_ratio() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _sample_aspect_ratio;
	}

	bool yuv() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _yuv;
	}

	Frame fade_in() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _fade_in;
	}

	Frame fade_out() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _fade_out;
	}

	VideoRange range() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _range;
	}

	PixelQuanta pixel_quanta() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _pixel_quanta;
	}

	bool has_alpha() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _has_alpha;
	}

	bool use() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _use;
	}

	boost::optional<dcp::LanguageTag> burnt_subtitle_language() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _burnt_subtitle_language;
	}


	/* XXX: names for these? */
	boost::optional<dcp::Size> size_after_3d_split() const;
	boost::optional<dcp::Size> size_after_crop() const;
	boost::optional<dcp::Size> scaled_size(dcp::Size container_size);

	boost::optional<double> fade(std::shared_ptr<const Film> film, dcpomatic::ContentTime time) const;

	std::string processing_description(std::shared_ptr<const Film> film);

	void set_length(Frame);

	void take_from_examiner(std::shared_ptr<const Film> film, std::shared_ptr<VideoExaminer>);
	void add_properties(std::list<UserProperty> &) const;

	void modify_position(std::shared_ptr<const Film> film, dcpomatic::DCPTime& pos) const;
	void modify_trim_start(dcpomatic::ContentTime& pos) const;

	void rotate_size();

	static std::shared_ptr<VideoContent> from_xml(Content* parent, cxml::ConstNodePtr node, int version, VideoRange video_range_hint);

private:

	friend struct ffmpeg_pts_offset_test;
	friend struct best_dcp_frame_rate_test_single;
	friend struct best_dcp_frame_rate_test_double;
	friend struct audio_sampling_rate_test;
	friend struct scaled_size_test1;
	friend struct scaled_size_test2;
	friend struct scaled_size_legacy_test;

	void setup_default_colour_conversion();

	bool _use;
	Frame _length;
	boost::optional<ColourConversion> _colour_conversion;
	boost::optional<dcp::Size> _size;
	VideoFrameType _frame_type;
	Crop _crop;
	/** ratio to scale cropped image to (or none to guess); i.e. if set, scale to _custom_ratio:1 */
	boost::optional<float> _custom_ratio;
	/** size to scale cropped image to; only used if _custom_ratio is none */
	boost::optional<dcp::Size> _custom_size;
	/** ratio obtained from an older metadata file; will be used to set up
	 *  _custom_{ratio,size} (or not, if not required) on the first call to
	 *  scaled_size()
	 */
	boost::optional<float> _legacy_ratio;
	/** Sample aspect ratio obtained from the content file's header, if there is one */
	boost::optional<double> _sample_aspect_ratio;
	bool _yuv;
	/** fade in time in content frames */
	Frame _fade_in;
	/** fade out time in content frames */
	Frame _fade_out;
	VideoRange _range;
	PixelQuanta _pixel_quanta;
	boost::optional<dcp::LanguageTag> _burnt_subtitle_language;
	bool _has_alpha = false;
};


#endif
