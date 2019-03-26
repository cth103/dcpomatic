/*
    Copyright (C) 2012-2019 Carl Hetherington <cth@carlh.net>

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

/** @file src/wx/wx_util.cc
 *  @brief Some utility functions and classes.
 */

#include "wx_util.h"
#include "file_picker_ctrl.h"
#include "static_text.h"
#include "password_entry.h"
#include "lib/config.h"
#include "lib/job_manager.h"
#include "lib/util.h"
#include "lib/cross.h"
#include "lib/job.h"
#include <dcp/locale_convert.h>
#include <wx/spinctrl.h>
#include <wx/splash.h>
#include <wx/progdlg.h>
#include <wx/filepicker.h>
#include <boost/thread.hpp>

using std::string;
using std::vector;
using std::pair;
using boost::shared_ptr;
using boost::optional;
using dcp::locale_convert;

wxStaticText *
#ifdef __WXOSX__
create_label (wxWindow* p, wxString t, bool left)
#else
create_label (wxWindow* p, wxString t, bool)
#endif
{
#ifdef __WXOSX__
	if (left) {
		t += wxT (":");
	}
#endif
	return new StaticText (p, t);
}

/** Add a wxStaticText to a wxSizer, aligning it at vertical centre.
 *  @param s Sizer to add to.
 *  @param p Parent window for the wxStaticText.
 *  @param t Text for the wxStaticText.
 *  @param left true if this label is a `left label'; ie the sort
 *  of label which should be right-aligned on OS X.
 *  @param prop Proportion to pass when calling Add() on the wxSizer.
 */
wxStaticText *
add_label_to_sizer (wxSizer* s, wxWindow* p, wxString t, bool left, int prop, int flags)
{
#ifdef __WXOSX__
	if (left) {
		flags |= wxALIGN_RIGHT;
	}
#endif
	wxStaticText* m = create_label (p, t, left);
	s->Add (m, prop, flags, 6);
	return m;
}

wxStaticText *
#ifdef __WXOSX__
add_label_to_sizer (wxSizer* s, wxStaticText* t, bool left, int prop, int flags)
#else
add_label_to_sizer (wxSizer* s, wxStaticText* t, bool, int prop, int flags)
#endif
{
#ifdef __WXOSX__
	if (left) {
		flags |= wxALIGN_RIGHT;
	}
#endif
	s->Add (t, prop, flags, 6);
	return t;
}

wxStaticText *
add_label_to_sizer (wxGridBagSizer* s, wxWindow* p, wxString t, bool left, wxGBPosition pos, wxGBSpan span)
{
	int flags = wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT;
#ifdef __WXOSX__
	if (left) {
		flags |= wxALIGN_RIGHT;
	}
#endif
	wxStaticText* m = create_label (p, t, left);
	s->Add (m, pos, span, flags);
	return m;
}

wxStaticText *
#ifdef __WXOSX__
add_label_to_sizer (wxGridBagSizer* s, wxStaticText* t, bool left, wxGBPosition pos, wxGBSpan span)
#else
add_label_to_sizer (wxGridBagSizer* s, wxStaticText* t, bool, wxGBPosition pos, wxGBSpan span)
#endif
{
	int flags = wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT;
#ifdef __WXOSX__
	if (left) {
		flags |= wxALIGN_RIGHT;
	}
#endif
	s->Add (t, pos, span, flags);
	return t;
}

/** Pop up an error dialogue box.
 *  @param parent Parent.
 *  @param m Message.
 *  @param e Extended message.
 */
void
error_dialog (wxWindow* parent, wxString m, optional<wxString> e)
{
	wxMessageDialog* d = new wxMessageDialog (parent, m, _("DCP-o-matic"), wxOK | wxICON_ERROR);
	if (e) {
		wxString em = *e;
		em[0] = wxToupper (em[0]);
		d->SetExtendedMessage (em);
	}
	d->ShowModal ();
	d->Destroy ();
}

/** Pop up an error dialogue box.
 *  @param parent Parent.
 *  @param m Message.
 */
void
message_dialog (wxWindow* parent, wxString m)
{
	wxMessageDialog* d = new wxMessageDialog (parent, m, _("DCP-o-matic"), wxOK | wxICON_INFORMATION);
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
	return string (s.ToUTF8 ());
}

/** @param s STL string.
 *  @return Corresponding wxWidgets string.
 */
wxString
std_to_wx (string s)
{
	return wxString (s.c_str(), wxConvUTF8);
}

string
string_client_data (wxClientData* o)
{
	return wx_to_std (dynamic_cast<wxStringClientData*>(o)->GetData());
}

void
checked_set (FilePickerCtrl* widget, boost::filesystem::path value)
{
	if (widget->GetPath() != std_to_wx (value.string())) {
		if (value.empty()) {
			/* Hack to make wxWidgets clear the control when we are passed
			   an empty value.
			*/
			value = " ";
		}
		widget->SetPath (std_to_wx (value.string()));
	}
}

void
checked_set (wxDirPickerCtrl* widget, boost::filesystem::path value)
{
	if (widget->GetPath() != std_to_wx (value.string())) {
		if (value.empty()) {
			/* Hack to make wxWidgets clear the control when we are passed
			   an empty value.
			*/
			value = " ";
		}
		widget->SetPath (std_to_wx (value.string()));
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
checked_set (wxSpinCtrlDouble* widget, double value)
{
	/* XXX: completely arbitrary epsilon */
	if (fabs (widget->GetValue() - value) > 1e-16) {
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
checked_set (wxChoice* widget, vector<pair<string, string> > items)
{
       vector<pair<string, string> > current;
       for (unsigned int i = 0; i < widget->GetCount(); ++i) {
               current.push_back (
                       make_pair (
                               wx_to_std (widget->GetString (i)),
                               string_client_data (widget->GetClientObject (i))
                               )
                       );
       }

       if (current == items) {
               return;
       }

       widget->Clear ();
       for (vector<pair<string, string> >::const_iterator i = items.begin(); i != items.end(); ++i) {
               widget->Append (std_to_wx (i->first), new wxStringClientData (std_to_wx (i->second)));
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
checked_set (PasswordEntry* entry, string value)
{
	if (entry->get() != value) {
		entry->set(value);
	}
}

void
checked_set (wxTextCtrl* widget, wxString value)
{
	if (widget->GetValue() != value) {
		widget->ChangeValue (value);
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
checked_set (wxStaticText* widget, wxString value)
{
	if (widget->GetLabel() != value) {
		widget->SetLabel (value);
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

#ifdef DCPOMATIC_LINUX
		locale->AddCatalogLookupPathPrefix (LINUX_LOCALE_PREFIX);

		/* We have to include the wxWidgets .mo in our distribution,
		   so we rename it to avoid clashes with any other installation
		   of wxWidgets.
		*/
		locale->AddCatalog (wxT ("dcpomatic2-wxstd"));

		/* Fedora 29 (at least) installs wxstd3.mo instead of wxstd.mo */
		locale->AddCatalog (wxT ("wxstd3"));
#endif

		locale->AddCatalog (wxT ("libdcpomatic2-wx"));
		locale->AddCatalog (wxT ("dcpomatic2"));

		if (!locale->IsOk()) {
			delete locale;
			locale = new wxLocale (wxLANGUAGE_ENGLISH);
		}
	}

	if (locale) {
		dcpomatic_setup_gettext_i18n (wx_to_std (locale->GetCanonicalName ()));
	}
}

int
wx_get (wxSpinCtrl* w)
{
	return w->GetValue ();
}

int
wx_get (wxChoice* w)
{
	return w->GetSelection ();
}

double
wx_get (wxSpinCtrlDouble* w)
{
	return w->GetValue ();
}

/** @param s String of the form Context|String
 *  @return translation, or String if no translation is available.
 */
wxString
context_translation (wxString s)
{
	wxString t = wxGetTranslation (s);
	if (t == s) {
		/* No translation; strip the context */
		int c = t.Find (wxT ("|"));
		if (c != wxNOT_FOUND) {
			t = t.Mid (c + 1);
		}
	}

	return t;
}

wxString
time_to_timecode (DCPTime t, double fps)
{
	double w = t.seconds ();
	int const h = (w / 3600);
	w -= h * 3600;
	int const m = (w / 60);
	w -= m * 60;
	int const s = floor (w);
	w -= s;
	int const f = lrint (w * fps);
	return wxString::Format (wxT("%02d:%02d:%02d.%02d"), h, m, s, f);
}

void
setup_audio_channels_choice (wxChoice* choice, int minimum)
{
	vector<pair<string, string> > items;
	for (int i = minimum; i <= 16; i += 2) {
		if (i == 2) {
			items.push_back (make_pair (wx_to_std (_("2 - stereo")), locale_convert<string> (i)));
		} else if (i == 4) {
			items.push_back (make_pair (wx_to_std (_("4 - L/C/R/Lfe")), locale_convert<string> (i)));
		} else if (i == 6) {
			items.push_back (make_pair (wx_to_std (_("6 - 5.1")), locale_convert<string> (i)));
		} else if (i == 8) {
			items.push_back (make_pair (wx_to_std (_("8 - 5.1/HI/VI")), locale_convert<string> (i)));
		} else if (i == 12) {
			items.push_back (make_pair (wx_to_std (_("12 - 7.1/HI/VI")), locale_convert<string> (i)));
		} else {
			items.push_back (make_pair (locale_convert<string> (i), locale_convert<string> (i)));
		}
	}

	checked_set (choice, items);
}

wxSplashScreen *
maybe_show_splash ()
{
	wxSplashScreen* splash = 0;
	try {
		wxBitmap bitmap;
		boost::filesystem::path p = shared_path () / "splash.png";
		if (bitmap.LoadFile (std_to_wx (p.string ()), wxBITMAP_TYPE_PNG)) {
			splash = new wxSplashScreen (bitmap, wxSPLASH_CENTRE_ON_SCREEN | wxSPLASH_NO_TIMEOUT, 0, 0, -1);
			wxYield ();
		}
	} catch (boost::filesystem::filesystem_error& e) {
		/* Maybe we couldn't find the splash image; never mind */
	}

	return splash;
}

double
calculate_mark_interval (double mark_interval)
{
	if (mark_interval > 5) {
		mark_interval -= lrint (mark_interval) % 5;
	}
	if (mark_interval > 10) {
		mark_interval -= lrint (mark_interval) % 10;
	}
	if (mark_interval > 60) {
		mark_interval -= lrint (mark_interval) % 60;
	}
	if (mark_interval > 3600) {
		mark_interval -= lrint (mark_interval) % 3600;
	}

	if (mark_interval < 1) {
		mark_interval = 1;
	}

	return mark_interval;
}


/** @return false if the task was cancelled */
bool
display_progress (wxString title, wxString task)
{
	JobManager* jm = JobManager::instance ();

	wxProgressDialog progress (title, task, 100, 0, wxPD_CAN_ABORT);

	bool ok = true;

	while (jm->work_to_do()) {
		dcpomatic_sleep (1);
		if (!progress.Pulse()) {
			/* user pressed cancel */
			BOOST_FOREACH (shared_ptr<Job> i, jm->get()) {
				i->cancel();
			}
			ok = false;
			break;
		}
	}

	return ok;
}

bool
report_errors_from_last_job (wxWindow* parent)
{
	JobManager* jm = JobManager::instance ();

	DCPOMATIC_ASSERT (!jm->get().empty());

	shared_ptr<Job> last = jm->get().back();
	if (last->finished_in_error()) {
		error_dialog(parent, std_to_wx(last->error_summary()) + ".\n", std_to_wx(last->error_details()));
		return false;
	}

	return true;
}
