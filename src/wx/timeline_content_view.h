/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_TIMELINE_CONTENT_VIEW_H
#define DCPOMATIC_TIMELINE_CONTENT_VIEW_H

#include "timeline_view.h"
#include <wx/wx.h>
#include <boost/signals2.hpp>

class Content;

/** @class TimelineContentView
 *  @brief Parent class for views of pieces of content.
 */
class TimelineContentView : public TimelineView
{
public:
	TimelineContentView (Timeline& tl, boost::shared_ptr<Content> c);

	dcpomatic::Rect<int> bbox () const;

	void set_selected (bool s);
	bool selected () const;
	boost::shared_ptr<Content> content () const;
	void set_track (int t);
	void unset_track ();
	boost::optional<int> track () const;

	virtual wxString type () const = 0;
	virtual wxColour background_colour () const = 0;
	virtual wxColour foreground_colour () const = 0;
	
private:

	void do_paint (wxGraphicsContext* gc);
	int y_pos (int t) const;
	void content_changed (int p, bool frequent);

	boost::weak_ptr<Content> _content;
	boost::optional<int> _track;
	bool _selected;

	boost::signals2::scoped_connection _content_connection;
};

typedef std::vector<boost::shared_ptr<TimelineContentView> > TimelineContentViewList;

#endif
