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

#ifndef DCPOMATIC_SUBTITLE_CONTENT_H
#define DCPOMATIC_SUBTITLE_CONTENT_H

#include "content_part.h"
#include <libcxml/cxml.h>
#include <dcp/types.h>
#include <boost/signals2.hpp>

class Font;

class SubtitleContentProperty
{
public:
	static int const SUBTITLE_X_OFFSET;
	static int const SUBTITLE_Y_OFFSET;
	static int const SUBTITLE_X_SCALE;
	static int const SUBTITLE_Y_SCALE;
	static int const USE_SUBTITLES;
	static int const BURN_SUBTITLES;
	static int const SUBTITLE_LANGUAGE;
	static int const FONTS;
	static int const SUBTITLE_VIDEO_FRAME_RATE;
	static int const SUBTITLE_COLOUR;
	static int const SUBTITLE_OUTLINE;
	static int const SUBTITLE_OUTLINE_COLOUR;
};

class SubtitleContent : public ContentPart
{
public:
	SubtitleContent (Content* parent, boost::shared_ptr<const Film>);
	SubtitleContent (Content* parent, boost::shared_ptr<const Film>, cxml::ConstNodePtr, int version);
	SubtitleContent (Content* parent, boost::shared_ptr<const Film>, std::vector<boost::shared_ptr<Content> >);

	void as_xml (xmlpp::Node *) const;
	std::string identifier () const;

	bool has_image_subtitles () const {
		/* XXX */
		return true;
	}

	void add_font (boost::shared_ptr<Font> font);

	void set_use_subtitles (bool);
	void set_burn_subtitles (bool);
	void set_subtitle_x_offset (double);
	void set_subtitle_y_offset (double);
	void set_subtitle_x_scale (double);
	void set_subtitle_y_scale (double);
	void set_subtitle_language (std::string language);

	bool use_subtitles () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _use_subtitles;
	}

	bool burn_subtitles () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _burn_subtitles;
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

	std::list<boost::shared_ptr<Font> > fonts () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _fonts;
	}

	std::string subtitle_language () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _subtitle_language;
	}

	void set_colour (dcp::Colour);

	dcp::Colour colour () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _colour;
	}

	void set_outline (bool);

	bool outline () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _outline;
	}

	void set_outline_colour (dcp::Colour);

	dcp::Colour outline_colour () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _outline_colour;
	}

	double subtitle_video_frame_rate () const;
	void set_subtitle_video_frame_rate (double r);

protected:
	/** subtitle language (e.g. "German") or empty if it is not known */
	std::string _subtitle_language;

private:
	friend struct ffmpeg_pts_offset_test;
	void font_changed ();
	void connect_to_fonts ();

	bool _use_subtitles;
	bool _burn_subtitles;
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
	std::list<boost::shared_ptr<Font> > _fonts;
	dcp::Colour _colour;
	bool _outline;
	dcp::Colour _outline_colour;
	std::list<boost::signals2::connection> _font_connections;
	/** Video frame rate that this content has been prepared for, if known */
	boost::optional<double> _frame_rate;
};

#endif
