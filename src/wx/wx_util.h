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

#ifndef DVDOMATIC_WX_UTIL_H
#define DVDOMATIC_WX_UTIL_H

#include <wx/wx.h>
#include <wx/gbsizer.h>
#include <boost/function.hpp>
#include <boost/thread.hpp>
#ifdef __WXGTK__
#include <gtk/gtk.h>
#endif

class wxFilePickerCtrl;
class wxSpinCtrl;
class wxGridBagSizer;

/** @file src/wx/wx_util.h
 *  @brief Some utility functions and classes.
 */

extern void error_dialog (wxWindow *, wxString);
extern bool confirm_dialog (wxWindow *, wxString);
extern wxStaticText* add_label_to_sizer (wxSizer *, wxWindow *, wxString, int prop = 0);
extern wxStaticText* add_label_to_grid_bag_sizer (wxGridBagSizer *, wxWindow *, wxString, wxGBPosition, wxGBSpan span = wxDefaultSpan);
extern std::string wx_to_std (wxString);
extern wxString std_to_wx (std::string);
extern void dvdomatic_setup_i18n ();

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

/* GTK 2.24.17 has a buggy GtkFileChooserButton and it was put in Ubuntu 13.04.
   Use our own dir picker as this is the least bad option I can think of.
*/
#if defined(__WXMSW__) || (GTK_MAJOR_VERSION == 2 && GTK_MINOR_VERSION == 24 && GTK_MICRO_VERSION == 17)
#define DVDOMATIC_USE_OWN_DIR_PICKER
#endif

#endif
