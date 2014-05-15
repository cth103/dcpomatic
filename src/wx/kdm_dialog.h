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
#include <boost/date_time/posix_time/posix_time.hpp>
#include <wx/wx.h>
#include <wx/treectrl.h>
#include "wx_util.h"

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
	KDMDialog (wxWindow *, boost::shared_ptr<const Film>);

	std::list<boost::shared_ptr<Screen> > screens () const;

	/** @return KDM from time in local time */
	boost::posix_time::ptime from () const;
	/** @return KDM until time in local time */
	boost::posix_time::ptime until () const;
	
	boost::filesystem::path dcp () const;
	boost::filesystem::path directory () const;
	bool write_to () const;

private:
	void add_cinema (boost::shared_ptr<Cinema>);
	void add_screen (boost::shared_ptr<Cinema>, boost::shared_ptr<Screen>);
	void add_cinema_clicked ();
	void edit_cinema_clicked ();
	void remove_cinema_clicked ();
	void add_screen_clicked ();
	void edit_screen_clicked ();
	void remove_screen_clicked ();
	std::list<std::pair<wxTreeItemId, boost::shared_ptr<Cinema> > > selected_cinemas () const;
	std::list<std::pair<wxTreeItemId, boost::shared_ptr<Screen> > > selected_screens () const;
	void setup_sensitivity ();

	static boost::posix_time::ptime posix_time (wxDatePickerCtrl *, wxTimePickerCtrl *);
	
	wxTreeCtrl* _targets;
	wxButton* _add_cinema;
	wxButton* _edit_cinema;
	wxButton* _remove_cinema;
	wxButton* _add_screen;
	wxButton* _edit_screen;
	wxButton* _remove_screen;
	wxDatePickerCtrl* _from_date;
	wxDatePickerCtrl* _until_date;
	wxTimePickerCtrl* _from_time;
	wxTimePickerCtrl* _until_time;
	wxListCtrl* _dcps;
	wxRadioButton* _write_to;
#ifdef DCPOMATIC_USE_OWN_DIR_PICKER
	DirPickerCtrl* _folder;
#else
	wxDirPickerCtrl* _folder;
#endif
	wxRadioButton* _email;

	wxTreeItemId _root;
	std::map<wxTreeItemId, boost::shared_ptr<Cinema> > _cinemas;
	std::map<wxTreeItemId, boost::shared_ptr<Screen> > _screens;
};
