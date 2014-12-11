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

#ifndef DCPOMATIC_SUBTITLE_CONTENT_H
#define DCPOMATIC_SUBTITLE_CONTENT_H

#include "content.h"

class SubtitleContentProperty
{
public:
	static int const SUBTITLE_X_OFFSET;
	static int const SUBTITLE_Y_OFFSET;
	static int const SUBTITLE_X_SCALE;
	static int const SUBTITLE_Y_SCALE;
	static int const USE_SUBTITLES;
	static int const SUBTITLE_LANGUAGE;
};

/** @class SubtitleContent
 *  @brief Parent for content which has the potential to include subtitles.
 *
 *  Although inheriting from this class indicates that the content could
 *  have subtitles, it may not.  ::has_subtitles() will tell you.
 */
class SubtitleContent : public virtual Content
{
public:
	SubtitleContent (boost::shared_ptr<const Film>);
	SubtitleContent (boost::shared_ptr<const Film>, boost::filesystem::path);
	SubtitleContent (boost::shared_ptr<const Film>, cxml::ConstNodePtr, int version);
	SubtitleContent (boost::shared_ptr<const Film>, std::vector<boost::shared_ptr<Content> >);

	void as_xml (xmlpp::Node *) const;
	std::string identifier () const;

	virtual bool has_subtitles () const = 0;

	void set_use_subtitles (bool);
	void set_subtitle_x_offset (double);
	void set_subtitle_y_offset (double);
	void set_subtitle_x_scale (double);
	void set_subtitle_y_scale (double);
	void set_subtitle_language (std::string language);

	bool use_subtitles () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _use_subtitles;
	}

	double subtitle_x_offset () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _subtitle_x_offset;
	}

	double subtitle_y_offset () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _subtitle_y_offset;
	}

	double subtitle_x_scale () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _subtitle_x_scale;
	}

	double subtitle_y_scale () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _subtitle_y_scale;
	}

	std::string subtitle_language () const {
		return _subtitle_language;
	}

protected:
	/** subtitle language (e.g. "German") or empty if it is not known */
	std::string _subtitle_language;
	
private:
	friend struct ffmpeg_pts_offset_test;

	bool _use_subtitles;
	/** x offset for placing subtitles, as a proportion of the container width;
	 * +ve is further right, -ve is further left.
	 */
	double _subtitle_x_offset;
	/** y offset for placing subtitles, as a proportion of the container height;
	 *  +ve is further down the frame, -ve is further up.
	 */
	double _subtitle_y_offset;
	/** x scale factor to apply to subtitles */
	double _subtitle_x_scale;
	/** y scale factor to apply to subtitles */
	double _subtitle_y_scale;
};

#endif
