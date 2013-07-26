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

/** @file src/wx/wx_util.cc
 *  @brief Some utility functions and classes.
 */

#include <boost/thread.hpp>
#include <wx/filepicker.h>
#include <wx/spinctrl.h>
#include "lib/config.h"
#include "lib/util.h"
#include "wx_util.h"

using namespace std;
using namespace boost;

/** Add a wxStaticText to a wxSizer, aligning it at vertical centre.
 *  @param s Sizer to add to.
 *  @param p Parent window for the wxStaticText.
 *  @param t Text for the wxStaticText.
 *  @param left true if this label is a `left label'; ie the sort
 *  of label which should be right-aligned on OS X.
 *  @param prop Proportion to pass when calling Add() on the wxSizer.
 */
wxStaticText *
#ifdef __WXOSX__
add_label_to_sizer (wxSizer* s, wxWindow* p, wxString t, bool left, int prop)
#else
add_label_to_sizer (wxSizer* s, wxWindow* p, wxString t, bool, int prop)
#endif
{
	int flags = wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT;
#ifdef __WXOSX__
	if (left) {
		flags |= wxALIGN_RIGHT;
		t += wxT (":");
	}
#endif	
	wxStaticText* m = new wxStaticText (p, wxID_ANY, t);
	s->Add (m, prop, flags, 6);
	return m;
}

wxStaticText *
#ifdef __WXOSX__
add_label_to_grid_bag_sizer (wxGridBagSizer* s, wxWindow* p, wxString t, bool left, wxGBPosition pos, wxGBSpan span)
#else
add_label_to_grid_bag_sizer (wxGridBagSizer* s, wxWindow* p, wxString t, bool, wxGBPosition pos, wxGBSpan span)
#endif
{
	int flags = wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT;
#ifdef __WXOSX__
	if (left) {
		flags |= wxALIGN_RIGHT;
		t += wxT (":");
	}
#endif	
	wxStaticText* m = new wxStaticText (p, wxID_ANY, t);
	s->Add (m, pos, span, flags);
	return m;
}

/** Pop up an error dialogue box.
 *  @param parent Parent.
 *  @param m Message.
 */
void
error_dialog (wxWindow* parent, wxString m)
{
	wxMessageDialog* d = new wxMessageDialog (parent, m, _("DCP-o-matic"), wxOK);
	d->ShowModal ();
	d->Destroy ();
}

bool
confirm_dialog (wxWindow* parent, wxString m)
{
	wxMessageDialog* d = new wxMessageDialog (parent, m, _("DCP-o-matic"), wxYES_NO | wxICON_QUESTION);
	int const r = d->ShowModal ();
	d->Destroy ();
	return r == wxID_YES;
}
	

/** @param s wxWidgets string.
 *  @return Corresponding STL string.
 */
string
wx_to_std (wxString s)
{
	return string (s.mb_str ());
}

/** @param s STL string.
 *  @return Corresponding wxWidgets string.
 */
wxString
std_to_wx (string s)
{
	return wxString (s.c_str(), wxConvUTF8);
}

int const ThreadedStaticText::_update_event_id = 10000;

/** @param parent Parent for the wxStaticText.
 *  @param initial Initial text for the wxStaticText while the computation is being run.
 *  @param fn Function which works out what the wxStaticText content should be and returns it.
 */
ThreadedStaticText::ThreadedStaticText (wxWindow* parent, wxString initial, function<string ()> fn)
	: wxStaticText (parent, wxID_ANY, initial)
{
	Bind (wxEVT_COMMAND_TEXT_UPDATED, boost::bind (&ThreadedStaticText::thread_finished, this, _1), _update_event_id);
	_thread = new thread (bind (&ThreadedStaticText::run, this, fn));
}

ThreadedStaticText::~ThreadedStaticText ()
{
	_thread->interrupt ();
	_thread->join ();
	delete _thread;
}

/** Run our thread and post the result to the GUI thread via AddPendingEvent */
void
ThreadedStaticText::run (function<string ()> fn)
{
	wxCommandEvent ev (wxEVT_COMMAND_TEXT_UPDATED, _update_event_id);
	ev.SetString (std_to_wx (fn ()));
	GetEventHandler()->AddPendingEvent (ev);
}

/** Called in the GUI thread when our worker thread has finished */
void
ThreadedStaticText::thread_finished (wxCommandEvent& ev)
{
	SetLabel (ev.GetString ());
}

string
string_client_data (wxClientData* o)
{
	return wx_to_std (dynamic_cast<wxStringClientData*>(o)->GetData());
}

void
checked_set (wxFilePickerCtrl* widget, string value)
{
	if (widget->GetPath() != std_to_wx (value)) {
		if (value.empty()) {
			/* Hack to make wxWidgets clear the control when we are passed
			   an empty value.
			*/
			value = " ";
		}
		widget->SetPath (std_to_wx (value));
	}
}

void
checked_set (wxSpinCtrl* widget, int value)
{
	if (widget->GetValue() != value) {
		widget->SetValue (value);
	}
}

void
checked_set (wxChoice* widget, int value)
{
	if (widget->GetSelection() != value) {
		widget->SetSelection (value);
	}
}

void
checked_set (wxChoice* widget, string value)
{
	wxClientData* o = 0;
	if (widget->GetSelection() != -1) {
		o = widget->GetClientObject (widget->GetSelection ());
	}
	
	if (!o || string_client_data(o) != value) {
		for (unsigned int i = 0; i < widget->GetCount(); ++i) {
			if (string_client_data (widget->GetClientObject (i)) == value) {
				widget->SetSelection (i);
			}
		}
	}
}

void
checked_set (wxTextCtrl* widget, string value)
{
	if (widget->GetValue() != std_to_wx (value)) {
		widget->ChangeValue (std_to_wx (value));
	}
}

void
checked_set (wxStaticText* widget, string value)
{
	if (widget->GetLabel() != std_to_wx (value)) {
		widget->SetLabel (std_to_wx (value));
	}
}

void
checked_set (wxCheckBox* widget, bool value)
{
	if (widget->GetValue() != value) {
		widget->SetValue (value);
	}
}

void
checked_set (wxRadioButton* widget, bool value)
{
	if (widget->GetValue() != value) {
		widget->SetValue (value);
	}
}

void
dcpomatic_setup_i18n ()
{
	int language = wxLANGUAGE_DEFAULT;

	boost::optional<string> config_lang = Config::instance()->language ();
	if (config_lang && !config_lang->empty ()) {
		wxLanguageInfo const * li = wxLocale::FindLanguageInfo (std_to_wx (config_lang.get ()));
		if (li) {
			language = li->Language;
		}
	}

	wxLocale* locale = 0;
	if (wxLocale::IsAvailable (language)) {
		locale = new wxLocale (language, wxLOCALE_LOAD_DEFAULT);

#ifdef DCPOMATIC_WINDOWS
		locale->AddCatalogLookupPathPrefix (std_to_wx (mo_path().string()));
#endif		

		locale->AddCatalog (wxT ("libdcpomatic-wx"));
		locale->AddCatalog (wxT ("dcpomatic"));
		
		if (!locale->IsOk()) {
			delete locale;
			locale = new wxLocale (wxLANGUAGE_ENGLISH);
		}
	}

	if (locale) {
		dcpomatic_setup_gettext_i18n (wx_to_std (locale->GetCanonicalName ()));
	}
}
