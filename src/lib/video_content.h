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

#ifndef DCPOMATIC_VIDEO_CONTENT_H
#define DCPOMATIC_VIDEO_CONTENT_H

#include "colour_conversion.h"
#include "video_content_scale.h"
#include "dcpomatic_time.h"
#include "user_property.h"
#include "types.h"
#include "content_part.h"
#include <boost/thread/mutex.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

class VideoExaminer;
class Ratio;
class Film;
class Content;

class VideoContentProperty
{
public:
	static int const SIZE;
	static int const FRAME_TYPE;
	static int const CROP;
	static int const SCALE;
	static int const COLOUR_CONVERSION;
	static int const FADE_IN;
	static int const FADE_OUT;
};

class VideoContent : public ContentPart, public boost::enable_shared_from_this<VideoContent>
{
public:
	VideoContent (Content* parent);
	VideoContent (Content* parent, std::vector<boost::shared_ptr<Content> >);

	void as_xml (xmlpp::Node *) const;
	std::string technical_summary () const;
	std::string identifier () const;
	void use_template (boost::shared_ptr<const VideoContent> c);

	Frame length () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _length;
	}

	Frame length_after_3d_combine () const {
		boost::mutex::scoped_lock lm (_mutex);
		if (_frame_type == VIDEO_FRAME_TYPE_3D_ALTERNATE) {
			return _length / 2;
		}

		return _length;
	}

	dcp::Size size () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _size;
	}

	void set_frame_type (VideoFrameType);

	void set_left_crop (int);
	void set_right_crop (int);
	void set_top_crop (int);
	void set_bottom_crop (int);

	void set_scale (VideoContentScale);
	void unset_colour_conversion ();
	void set_colour_conversion (ColourConversion);

	void set_fade_in (Frame);
	void set_fade_out (Frame);

	VideoFrameType frame_type () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _frame_type;
	}

	Crop crop () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _crop;
	}

	int left_crop () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _crop.left;
	}

	int right_crop () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _crop.right;
	}

	int top_crop () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _crop.top;
	}

	int bottom_crop () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _crop.bottom;
	}

	/** @return Description of how to scale this content (if indeed it should be scaled) */
	VideoContentScale scale () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _scale;
	}

	boost::optional<ColourConversion> colour_conversion () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _colour_conversion;
	}

	boost::optional<double> sample_aspect_ratio () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _sample_aspect_ratio;
	}

	bool yuv () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _yuv;
	}

	Frame fade_in () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _fade_in;
	}

	Frame fade_out () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _fade_out;
	}

	dcp::Size size_after_3d_split () const;
	dcp::Size size_after_crop () const;

	boost::optional<double> fade (Frame) const;

	void scale_and_crop_to_fit_width ();
	void scale_and_crop_to_fit_height ();

	std::string processing_description () const;

	void set_length (Frame);

	void take_from_examiner (boost::shared_ptr<VideoExaminer>);
	void add_properties (std::list<UserProperty> &) const;

	void modify_position (DCPTime& pos) const;
	void modify_trim_start (ContentTime& pos) const;

	static boost::shared_ptr<VideoContent> from_xml (Content* parent, cxml::ConstNodePtr, int);

private:

	friend struct ffmpeg_pts_offset_test;
	friend struct best_dcp_frame_rate_test_single;
	friend struct best_dcp_frame_rate_test_double;
	friend struct audio_sampling_rate_test;

	VideoContent (Content* parent, cxml::ConstNodePtr, int);
	void setup_default_colour_conversion ();

	Frame _length;
	boost::optional<ColourConversion> _colour_conversion;
	dcp::Size _size;
	VideoFrameType _frame_type;
	Crop _crop;
	VideoContentScale _scale;
	/** Sample aspect ratio obtained from the content file's header, if there is one */
	boost::optional<double> _sample_aspect_ratio;
	bool _yuv;
	Frame _fade_in;
	Frame _fade_out;
};

#endif
