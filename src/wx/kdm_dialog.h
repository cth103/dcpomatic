/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

#include <map>
#include <boost/shared_ptr.hpp>
#include <wx/wx.h>
#include <wx/treectrl.h>

class wxTreeCtrl;
class wxDatePickerCtrl;
class wxTimePickerCtrl;
class wxDirPickerCtrl;
class DirPickerCtrl;

class Cinema;
class Screen;

class KDMDialog : public wxDialog
{
public:
	KDMDialog (wxWindow *);

private:
	void add_cinema (boost::shared_ptr<Cinema>);
	void add_screen (boost::shared_ptr<Cinema>, boost::shared_ptr<Screen>);
	void targets_selection_changed (wxCommandEvent &);
	void new_cinema_clicked (wxCommandEvent &);
	void new_screen_clicked (wxCommandEvent &);
	
	wxTreeCtrl* _targets;
	wxButton* _new_cinema;
	wxButton* _new_screen;
	wxDatePickerCtrl* _from_date;
	wxDatePickerCtrl* _to_date;
	wxTimePickerCtrl* _from_time;
	wxTimePickerCtrl* _to_time;
#ifdef __WXMSW__	
	DirPickerCtrl* _folder;
#else
	wxDirPickerCtrl* _folder;
#endif

	wxTreeItemId _root;
	std::map<wxTreeItemId, boost::shared_ptr<Cinema> > _cinemas;
	std::map<wxTreeItemId, boost::shared_ptr<Screen> > _screens;
};
