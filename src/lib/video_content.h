/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_VIDEO_CONTENT_H
#define DCPOMATIC_VIDEO_CONTENT_H

#include "colour_conversion.h"
#include "video_content_scale.h"

class VideoExaminer;
class Ratio;

class VideoContentProperty
{
public:
	static int const VIDEO_SIZE;
	static int const VIDEO_FRAME_RATE;
	static int const VIDEO_FRAME_TYPE;
	static int const VIDEO_CROP;
	static int const VIDEO_SCALE;
	static int const COLOUR_CONVERSION;
	static int const VIDEO_FADE_IN;
	static int const VIDEO_FADE_OUT;
};

class VideoContent
{
public:
	VideoContent (boost::shared_ptr<const Film>);
	VideoContent (boost::shared_ptr<const Film>, cxml::ConstNodePtr, int);
	VideoContent (boost::shared_ptr<const Film>, std::vector<boost::shared_ptr<Content> >);

	void as_xml (xmlpp::Node *) const;
	std::string technical_summary () const;
	std::string identifier () const;

	void set_default_colour_conversion ();

	Frame video_length () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _video_length;
	}

	Frame video_length_after_3d_combine () const {
		boost::mutex::scoped_lock lm (_mutex);
		if (_video_frame_type == VIDEO_FRAME_TYPE_3D_ALTERNATE) {
			return _video_length / 2;
		}

		return _video_length;
	}

	dcp::Size video_size () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _video_size;
	}

	double video_frame_rate () const;

	/** @return true if this content has a specific video frame rate, false
	 *  if it should use the DCP's rate.
	 */
	bool has_own_video_frame_rate () const {
		boost::mutex::scoped_lock lm (_mutex);
		return static_cast<bool>(_video_frame_rate);
	}

	void set_video_frame_type (VideoFrameType);
	void set_video_frame_rate (double);

	void set_left_crop (int);
	void set_right_crop (int);
	void set_top_crop (int);
	void set_bottom_crop (int);

	void set_scale (VideoContentScale);
	void unset_colour_conversion ();
	void set_colour_conversion (ColourConversion);

	void set_fade_in (Frame);
	void set_fade_out (Frame);

	VideoFrameType video_frame_type () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _video_frame_type;
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

	dcp::Size video_size_after_3d_split () const;
	dcp::Size video_size_after_crop () const;

	ContentTime dcp_time_to_content_time (DCPTime) const;

	boost::optional<double> fade (Frame) const;

	void scale_and_crop_to_fit_width ();
	void scale_and_crop_to_fit_height ();

	std::string processing_description () const;

private:
	void take_from_video_examiner (boost::shared_ptr<VideoExaminer>);
	void add_properties (std::list<UserProperty> &) const;

	boost::weak_ptr<const Film> _film;
	boost::mutex _mutex;
	Frame _video_length;
	/** Video frame rate, or not set if this content should use the DCP's frame rate */
	boost::optional<double> _video_frame_rate;
	boost::optional<ColourConversion> _colour_conversion;

	friend struct ffmpeg_pts_offset_test;
	friend struct best_dcp_frame_rate_test_single;
	friend struct best_dcp_frame_rate_test_double;
	friend struct audio_sampling_rate_test;

	void setup_default_colour_conversion ();

	dcp::Size _video_size;
	VideoFrameType _video_frame_type;
	Crop _crop;
	VideoContentScale _scale;
	/** Sample aspect ratio obtained from the content file's header,
	    if there is one.
	*/
	boost::optional<double> _sample_aspect_ratio;
	bool _yuv;
	Frame _fade_in;
	Frame _fade_out;
};

#endif
