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

#ifndef DCPOMATIC_TIMELINE_CONTENT_VIEW_H
#define DCPOMATIC_TIMELINE_CONTENT_VIEW_H

#include "lib/types.h"
#include "lib/warnings.h"
#include "timeline_view.h"
DCPOMATIC_DISABLE_WARNINGS
#include <wx/wx.h>
DCPOMATIC_ENABLE_WARNINGS
#include <boost/signals2.hpp>

class Content;

/** @class TimelineContentView
 *  @brief Parent class for views of pieces of content.
 */
class TimelineContentView : public TimelineView
{
public:
	TimelineContentView (Timeline& tl, std::shared_ptr<Content> c);

	dcpomatic::Rect<int> bbox () const;

	void set_selected (bool s);
	bool selected () const;
	std::shared_ptr<Content> content () const;
	void set_track (int t);
	void unset_track ();
	boost::optional<int> track () const;

	virtual bool active () const = 0;
	virtual wxColour background_colour () const = 0;
	virtual wxColour foreground_colour () const = 0;
	virtual wxString label () const;

protected:

	std::weak_ptr<Content> _content;

private:

	void do_paint (wxGraphicsContext* gc, std::list<dcpomatic::Rect<int> > overlaps);
	int y_pos (int t) const;
	void content_change (ChangeType type, int p);

	boost::optional<int> _track;
	bool _selected;

	boost::signals2::scoped_connection _content_connection;
};

typedef std::vector<std::shared_ptr<TimelineContentView> > TimelineContentViewList;

#endif
