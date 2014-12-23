/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

#include "content.h"
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

class VideoContent : public virtual Content
{
public:
	VideoContent (boost::shared_ptr<const Film>);
	VideoContent (boost::shared_ptr<const Film>, DCPTime, ContentTime);
	VideoContent (boost::shared_ptr<const Film>, boost::filesystem::path);
	VideoContent (boost::shared_ptr<const Film>, cxml::ConstNodePtr, int);
	VideoContent (boost::shared_ptr<const Film>, std::vector<boost::shared_ptr<Content> >);

	void as_xml (xmlpp::Node *) const;
	std::string technical_summary () const;
	virtual std::string identifier () const;

	ContentTime video_length () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _video_length;
	}

	ContentTime video_length_after_3d_combine () const {
		boost::mutex::scoped_lock lm (_mutex);
		if (_video_frame_type == VIDEO_FRAME_TYPE_3D_ALTERNATE) {
			return ContentTime (_video_length.get() / 2);
		}
		
		return _video_length;
	}

	dcp::Size video_size () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _video_size;
	}
	
	float video_frame_rate () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _video_frame_rate;
	}

	void set_video_frame_type (VideoFrameType);
	void set_video_frame_rate (float);

	void set_left_crop (int);
	void set_right_crop (int);
	void set_top_crop (int);
	void set_bottom_crop (int);

	void set_scale (VideoContentScale);
	void unset_colour_conversion ();
	void set_colour_conversion (ColourConversion);
	void set_default_colour_conversion (bool signal = true);

	void set_fade_in (ContentTime);
	void set_fade_out (ContentTime);
	
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

	boost::optional<float> sample_aspect_ratio () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _sample_aspect_ratio;
	}

	ContentTime fade_in () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _fade_in;
	}

	ContentTime fade_out () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _fade_out;
	}
	
	dcp::Size video_size_after_3d_split () const;
	dcp::Size video_size_after_crop () const;

	ContentTime dcp_time_to_content_time (DCPTime) const;

	boost::optional<float> fade (VideoFrame) const;

	void scale_and_crop_to_fit_width ();
	void scale_and_crop_to_fit_height ();

	std::string processing_description () const;

protected:
	void take_from_video_examiner (boost::shared_ptr<VideoExaminer>);

	ContentTime _video_length;
	float _video_frame_rate;

private:
	friend struct ffmpeg_pts_offset_test;
	friend struct best_dcp_frame_rate_test_single;
	friend struct best_dcp_frame_rate_test_double;
	friend struct audio_sampling_rate_test;

	void setup_default_colour_conversion ();
	
	dcp::Size _video_size;
	VideoFrameType _video_frame_type;
	Crop _crop;
	VideoContentScale _scale;
	boost::optional<ColourConversion> _colour_conversion;
	/** Sample aspect ratio obtained from the content file's header,
	    if there is one.
	*/
	boost::optional<float> _sample_aspect_ratio;
	ContentTime _fade_in;
	ContentTime _fade_out;
};

#endif
