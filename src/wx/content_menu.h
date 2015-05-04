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

#ifndef DCPOMATIC_CONTENT_MENU_H
#define DCPOMATIC_CONTENT_MENU_H

#include "timeline_content_view.h"
#include "lib/types.h"
#include <wx/wx.h>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

class Film;
class Job;

class ContentMenu : public boost::noncopyable
{
public:
	ContentMenu (wxWindow* p);
	~ContentMenu ();
	
	void popup (boost::weak_ptr<Film>, ContentList, TimelineContentViewList, wxPoint);

private:
	void repeat ();
	void join ();
	void find_missing ();
	void properties ();
	void re_examine ();
	void kdm ();
	void remove ();
	void maybe_found_missing (boost::weak_ptr<Job>, boost::weak_ptr<Content>, boost::weak_ptr<Content>);
	
	wxMenu* _menu;
	/** Film that we are working with; set up by popup() */
	boost::weak_ptr<Film> _film;
	wxWindow* _parent;
	ContentList _content;
	TimelineContentViewList _views;
	wxMenuItem* _repeat;
	wxMenuItem* _join;
	wxMenuItem* _find_missing;
	wxMenuItem* _properties;
	wxMenuItem* _re_examine;
	wxMenuItem* _kdm;
	wxMenuItem* _remove;

	boost::signals2::scoped_connection _job_connection;
};

#endif
