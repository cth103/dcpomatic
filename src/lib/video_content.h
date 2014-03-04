/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

class VideoExaminer;
class Ratio;

class VideoContentProperty
{
public:
	static int const VIDEO_SIZE;
	static int const VIDEO_FRAME_RATE;
	static int const VIDEO_FRAME_TYPE;
	static int const VIDEO_CROP;
	static int const VIDEO_RATIO;
	static int const COLOUR_CONVERSION;
};

class VideoContent : public virtual Content
{
public:
	typedef int Frame;

	VideoContent (boost::shared_ptr<const Film>);
	VideoContent (boost::shared_ptr<const Film>, DCPTime, ContentTime);
	VideoContent (boost::shared_ptr<const Film>, boost::filesystem::path);
	VideoContent (boost::shared_ptr<const Film>, boost::shared_ptr<const cxml::Node>);
	VideoContent (boost::shared_ptr<const Film>, std::vector<boost::shared_ptr<Content> >);

	void as_xml (xmlpp::Node *) const;
	std::string technical_summary () const;
	virtual std::string information () const;
	virtual std::string identifier () const;

	ContentTime video_length () const {
		boost::mutex::scoped_lock lm (_mutex);
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

	void set_left_crop (int);
	void set_right_crop (int);
	void set_top_crop (int);
	void set_bottom_crop (int);

	void set_colour_conversion (ColourConversion);

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
	
	void set_ratio (Ratio const *);

	/** @return ratio to scale to, or 0 if the content's own ratio should be preserved. */
	Ratio const * ratio () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _ratio;
	}

	ColourConversion colour_conversion () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _colour_conversion;
	}

	dcp::Size video_size_after_3d_split () const;
	dcp::Size video_size_after_crop () const;

	ContentTime dcp_time_to_content_time (DCPTime) const;

protected:
	void take_from_video_examiner (boost::shared_ptr<VideoExaminer>);

	ContentTime _video_length;
	float _video_frame_rate;

private:
	friend class ffmpeg_pts_offset_test;
	friend class best_dcp_frame_rate_test_single;
	friend class best_dcp_frame_rate_test_double;
	friend class audio_sampling_rate_test;

	void setup_default_colour_conversion ();
	
	dcp::Size _video_size;
	VideoFrameType _video_frame_type;
	Crop _crop;
	Ratio const * _ratio;
	ColourConversion _colour_conversion;
};

#endif
