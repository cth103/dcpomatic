/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

/** @file src/wx/wx_util.h
 *  @brief Some utility functions and classes.
 */

#ifndef DCPOMATIC_WX_UTIL_H
#define DCPOMATIC_WX_UTIL_H

#include <wx/wx.h>
#include <wx/gbsizer.h>
#include <boost/function.hpp>
#include <boost/thread.hpp>
#include <boost/signals2.hpp>
#ifdef __WXGTK__
#include <gtk/gtk.h>
#endif

class wxFilePickerCtrl;
class wxSpinCtrl;
class wxSpinCtrlDouble;
class wxGridBagSizer;

#define DCPOMATIC_SIZER_X_GAP 8
#define DCPOMATIC_SIZER_Y_GAP 8
#define DCPOMATIC_SIZER_GAP 8
#define DCPOMATIC_DIALOG_BORDER 12

/** Spacing to use between buttons in a vertical line */
#ifdef DCPOMATIC_OSX
#define DCPOMATIC_BUTTON_STACK_GAP 2
#else
#define DCPOMATIC_BUTTON_STACK_GAP 0
#endif

/** i18n macro to support strings like Context|String
 *  so that `String' can be translated to different things
 *  in different contexts.
 */
#define S_(x) context_translation(x)

extern void error_dialog (wxWindow *, wxString);
extern bool confirm_dialog (wxWindow *, wxString);
extern wxStaticText* add_label_to_sizer (wxSizer *, wxWindow *, wxString, bool left, int prop = 0);
extern wxStaticText* add_label_to_grid_bag_sizer (wxGridBagSizer *, wxWindow *, wxString, bool, wxGBPosition, wxGBSpan span = wxDefaultSpan);
extern std::string wx_to_std (wxString);
extern wxString std_to_wx (std::string);
extern void dcpomatic_setup_i18n ();
extern wxString context_translation (wxString);
extern std::string string_client_data (wxClientData* o);

extern void checked_set (wxFilePickerCtrl* widget, std::string value);
extern void checked_set (wxSpinCtrl* widget, int value);
extern void checked_set (wxSpinCtrlDouble* widget, double value);
extern void checked_set (wxChoice* widget, int value);
extern void checked_set (wxChoice* widget, std::string value);
extern void checked_set (wxChoice* widget, std::vector<std::pair<std::string, std::string> > items);
extern void checked_set (wxTextCtrl* widget, std::string value);
extern void checked_set (wxTextCtrl* widget, wxString value);
extern void checked_set (wxCheckBox* widget, bool value);
extern void checked_set (wxRadioButton* widget, bool value);
extern void checked_set (wxStaticText* widget, std::string value);
extern void checked_set (wxStaticText* widget, wxString value);

extern int wx_get (wxChoice* widget);
extern int wx_get (wxSpinCtrl* widget);
extern double wx_get (wxSpinCtrlDouble* widget);

/* GTK 2.24.17 has a buggy GtkFileChooserButton and it was put in Ubuntu 13.04.
   This also seems to apply to 2.24.20 in Ubuntu 13.10 and 2.24.23 in Ubuntu 14.04.
   Use our own dir picker as this is the least bad option I can think of.
*/
#if defined(__WXMSW__) || (GTK_MAJOR_VERSION == 2 && GTK_MINOR_VERSION == 24 && (GTK_MICRO_VERSION == 17 || GTK_MICRO_VERSION == 20 || GTK_MICRO_VERSION == 23))
#define DCPOMATIC_USE_OWN_DIR_PICKER
#endif

#endif
