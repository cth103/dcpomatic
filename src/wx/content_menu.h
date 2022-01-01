/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_CONTENT_MENU_H
#define DCPOMATIC_CONTENT_MENU_H


#include "timeline_content_view.h"
#include "lib/types.h"
#include <wx/wx.h>
#include <memory>


class AutoCropDialog;
class DCPContent;
class Film;
class FilmViewer;
class Job;


class ContentMenu
{
public:
	ContentMenu (wxWindow* parent, std::weak_ptr<FilmViewer> viewer);

	ContentMenu (ContentMenu const &) = delete;
	ContentMenu& operator= (ContentMenu const &) = delete;

	void popup (std::weak_ptr<Film>, ContentList, TimelineContentViewList, wxPoint);

private:
	void repeat ();
	void join ();
	void find_missing ();
	void properties ();
	void advanced ();
	void re_examine ();
	void auto_crop ();
	void kdm ();
	void ov ();
	void set_dcp_settings ();
	void remove ();
	void cpl_selected (wxCommandEvent& ev);

	wxMenu* _menu;
	wxMenu* _cpl_menu;
	/** Film that we are working with; set up by popup() */
	std::weak_ptr<Film> _film;
	wxWindow* _parent;
	bool _pop_up_open;
	std::weak_ptr<FilmViewer> _viewer;
	ContentList _content;
	TimelineContentViewList _views;
	wxMenuItem* _repeat;
	wxMenuItem* _join;
	wxMenuItem* _find_missing;
	wxMenuItem* _properties;
	wxMenuItem* _advanced;
	wxMenuItem* _re_examine;
	wxMenuItem* _auto_crop;
	wxMenuItem* _kdm;
	wxMenuItem* _ov;
	wxMenuItem* _choose_cpl;
	wxMenuItem* _set_dcp_settings;
	wxMenuItem* _remove;

	AutoCropDialog* _auto_crop_dialog = nullptr;
	boost::signals2::scoped_connection _auto_crop_config_connection;
	boost::signals2::scoped_connection _auto_crop_viewer_connection;
};


#endif
