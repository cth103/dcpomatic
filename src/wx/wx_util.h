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

#include <wx/wx.h>
#include <boost/function.hpp>
#include <boost/thread.hpp>

class wxFilePickerCtrl;
class wxSpinCtrl;

/** @file src/wx/wx_util.h
 *  @brief Some utility functions and classes.
 */

extern void error_dialog (wxWindow *, wxString);
extern wxStaticText* add_label_to_sizer (wxSizer *, wxWindow *, wxString, int prop = 0);
extern std::string wx_to_std (wxString);
extern wxString std_to_wx (std::string);

/** @class ThreadedStaticText
 *
 *  @brief A wxStaticText whose content is computed in a separate thread, to avoid holding
 *  up the GUI while work is done.
 */
class ThreadedStaticText : public wxStaticText
{
public:
	ThreadedStaticText (wxWindow* parent, wxString initial, boost::function<std::string ()> fn);
	~ThreadedStaticText ();

private:
	void run (boost::function<std::string ()> fn);
	void thread_finished (wxCommandEvent& ev);

	/** Thread to do our work in */
	boost::thread* _thread;
	
	static const int _update_event_id;
};

extern std::string string_client_data (wxClientData* o);

extern void checked_set (wxFilePickerCtrl* widget, std::string value);
extern void checked_set (wxSpinCtrl* widget, int value);
extern void checked_set (wxChoice* widget, int value);
extern void checked_set (wxChoice* widget, std::string value);
extern void checked_set (wxTextCtrl* widget, std::string value);
extern void checked_set (wxCheckBox* widget, bool value);
extern void checked_set (wxRadioButton* widget, bool value);
extern void checked_set (wxStaticText* widget, std::string value);
