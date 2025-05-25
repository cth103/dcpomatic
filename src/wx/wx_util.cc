/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include "file_picker_ctrl.h"
#include "language_tag_widget.h"
#include "password_entry.h"
#include "region_subtag_widget.h"
#include "static_text.h"
#include "wx_util.h"
#include "wx_variant.h"
#include "lib/config.h"
#include "lib/cross.h"
#include "lib/job.h"
#include "lib/job_manager.h"
#include "lib/util.h"
#include "lib/variant.h"
#include "lib/version.h"
#include <dcp/locale_convert.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/display.h>
#include <wx/filepicker.h>
#include <wx/progdlg.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/splash.h>
#if wxCHECK_VERSION(3, 1, 6)
#include <wx/uilocale.h>
#endif
LIBDCP_ENABLE_WARNINGS
#include <boost/thread.hpp>


using std::string;
using std::vector;
using std::pair;
using std::shared_ptr;
using boost::optional;
using dcp::locale_convert;
using namespace dcpomatic;


wxStaticText *
#ifdef __WXOSX__
create_label (wxWindow* p, wxString t, bool left)
#else
create_label (wxWindow* p, wxString t, bool)
#endif
{
#ifdef __WXOSX__
	if (left) {
		t += char_to_wx(":");
	}
#endif
	return new StaticText (p, t);
}


#ifdef __WXOSX__
static
void
setup_osx_flags (wxSizer* s, bool left, int& flags)
{
	if (left) {
		auto box = dynamic_cast<wxBoxSizer*>(s);
		if (!box || box->GetOrientation() != wxHORIZONTAL) {
			flags |= wxALIGN_RIGHT;
		}
	}
}
#endif


/** Add a wxStaticText to a wxSizer.
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
	setup_osx_flags (s, left, flags);
#endif
	auto m = create_label (p, t, left);
	s->Add (m, prop, flags, DCPOMATIC_SIZER_GAP);
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
	setup_osx_flags (s, left, flags);
#endif
	s->Add (t, prop, flags, DCPOMATIC_SIZER_GAP);
	return t;
}


wxStaticText *
add_label_to_sizer(wxGridBagSizer* s, wxWindow* p, wxString t, bool left, wxGBPosition pos, wxGBSpan span, bool indent)
{
	int flags = wxALIGN_CENTER_VERTICAL | wxLEFT;
#ifdef __WXOSX__
	setup_osx_flags (s, left, flags);
#endif
	auto m = create_label (p, t, left);
	s->Add(m, pos, span, flags, indent ? DCPOMATIC_SIZER_X_GAP : 0);
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
	setup_osx_flags (s, left, flags);
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
	wxMessageDialog dialog(parent, m, variant::wx::dcpomatic(), wxOK | wxICON_ERROR);
	if (e) {
		wxString em = *e;
		em[0] = wxToupper (em[0]);
		dialog.SetExtendedMessage(em);
	}
	dialog.ShowModal();
}


/** Pop up an error dialogue box.
 *  @param parent Parent.
 *  @param m Message.
 */
void
message_dialog (wxWindow* parent, wxString m)
{
	wxMessageDialog dialog(parent, m, variant::wx::dcpomatic(), wxOK | wxICON_INFORMATION);
	dialog.ShowModal();
}


/** @return true if the user answered "yes" */
bool
confirm_dialog (wxWindow* parent, wxString m)
{
	wxMessageDialog dialog(parent, m, variant::wx::dcpomatic(), wxYES_NO | wxICON_QUESTION);
	return dialog.ShowModal() == wxID_YES;
}


/** @param s wxWidgets string.
 *  @return Corresponding STL string.
 */
string
wx_to_std (wxString s)
{
	return string (s.ToUTF8());
}


/** @param s STL string.
 *  @return Corresponding wxWidgets string.
 */
wxString
std_to_wx (string s)
{
	return wxString (s.c_str(), wxConvUTF8);
}


wxString
char_to_wx(char const* s)
{
	return wxString::FromUTF8(s);
}


string
string_client_data (wxClientData* o)
{
	return wx_to_std (dynamic_cast<wxStringClientData*>(o)->GetData());
}


void
checked_set (FilePickerCtrl* widget, boost::filesystem::path value)
{
	if (widget->path() != value) {
		if (value.empty()) {
			/* Hack to make wxWidgets clear the control when we are passed
			   an empty value.
			*/
			value = " ";
		}
		widget->set_path(value);
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
	wxClientData* o = nullptr;
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
checked_set (wxChoice* widget, vector<pair<string, string>> items)
{
       vector<pair<string, string>> current;
       for (unsigned int i = 0; i < widget->GetCount(); ++i) {
               current.push_back (
                       make_pair(
                               wx_to_std(widget->GetString(i)),
                               widget->GetClientData() ? string_client_data(widget->GetClientObject(i)) : ""
                               )
                       );
       }

       if (current == items) {
               return;
       }

       widget->Clear ();
       for (auto i: items) {
               widget->Append (std_to_wx(i.first), new wxStringClientData(std_to_wx(i.second)));
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
checked_set(LanguageTagWidget* widget, dcp::LanguageTag value)
{
	if (widget->get() != value) {
		widget->set(value);
	}
}


void
checked_set(LanguageTagWidget* widget, optional<dcp::LanguageTag> value)
{
	if (widget->get() != value) {
		widget->set(value);
	}
}


void
checked_set(RegionSubtagWidget* widget, optional<dcp::LanguageTag::RegionSubtag> value)
{
	if (widget->get() != value) {
		widget->set(value);
	}
}


#ifdef DCPOMATIC_OSX

void
dcpomatic_setup_i18n()
{
	wxLog::EnableLogging();

#if wxCHECK_VERSION(3, 1, 6)
	wxUILocale::UseDefault();
#endif

	auto get_locale_value = [](CFLocaleKey key) {
		CFLocaleRef cflocale = CFLocaleCopyCurrent();
		auto value = (CFStringRef) CFLocaleGetValue(cflocale, key);
		char buffer[64];
		CFStringGetCString(value, buffer, sizeof(buffer), kCFStringEncodingUTF8);
		CFRelease(cflocale);
		return string(buffer);
	};

	auto translations = new wxTranslations();

	auto config_lang = Config::instance()->language();
	if (config_lang && !config_lang->empty()) {
		translations->SetLanguage(std_to_wx(*config_lang));
	} else {
		/* We want to use the user's preferred language.  It seems that if we use the wxWidgets default we will get the
		 * language for the locale, which may not be what we want (e.g. for a machine in Germany, configured for DE locale,
		 * but with the preferred language set to English).
		 *
		 * Instead, the the language code from macOS then get the corresponding canonical language string with region,
		 * which wxTranslations::SetLanguage will accept.
		 */
		auto const language_code = get_locale_value(kCFLocaleLanguageCode);
		/* Ideally this would be wxUILocale (as wxLocale is deprecated) but we want to keep this building
		 * with the old wxWidgets we use for the older macOS builds.
		 */
		auto const info = wxLocale::FindLanguageInfo(std_to_wx(language_code));
		if (info) {
#if wxCHECK_VERSION(3, 1, 6)
			translations->SetLanguage(info->GetCanonicalWithRegion());
#else
			translations->SetLanguage(info->CanonicalName);
#endif
		}
	}

#ifdef DCPOMATIC_DEBUG
	wxFileTranslationsLoader::AddCatalogLookupPathPrefix(char_to_wx("build/src/wx/mo"));
	wxFileTranslationsLoader::AddCatalogLookupPathPrefix(char_to_wx("build/src/tools/mo"));
#endif

	translations->AddStdCatalog();
	translations->AddCatalog(char_to_wx("libdcpomatic2-wx"));
	translations->AddCatalog(char_to_wx("dcpomatic2"));

	wxTranslations::Set(translations);

	dcpomatic_setup_gettext_i18n(config_lang.get_value_or(""));
}

#else

void
dcpomatic_setup_i18n ()
{
	int language = wxLANGUAGE_DEFAULT;

	auto config_lang = Config::instance()->language ();
	if (config_lang && !config_lang->empty ()) {
		auto const li = wxLocale::FindLanguageInfo (std_to_wx (config_lang.get ()));
		if (li) {
			language = li->Language;
		}
	}

	wxLocale* locale = nullptr;
	if (wxLocale::IsAvailable (language)) {
		locale = new wxLocale (language, wxLOCALE_LOAD_DEFAULT);

#ifdef DCPOMATIC_WINDOWS
		locale->AddCatalogLookupPathPrefix (std_to_wx (mo_path().string()));
		locale->AddCatalog(char_to_wx("wxstd-3.1"));
#endif

#ifdef DCPOMATIC_LINUX
		locale->AddCatalogLookupPathPrefix(std_to_wx(LINUX_LOCALE_PREFIX));

		/* We have to include the wxWidgets .mo in our distribution,
		   so we rename it to avoid clashes with any other installation
		   of wxWidgets.
		*/
		locale->AddCatalog(char_to_wx("dcpomatic2-wxstd"));

		/* Fedora 29 (at least) installs wxstd3.mo instead of wxstd.mo */
		locale->AddCatalog(char_to_wx("wxstd3"));
#endif

		locale->AddCatalog(char_to_wx("wxstd"));
		locale->AddCatalog(char_to_wx("libdcpomatic2-wx"));
		locale->AddCatalog(char_to_wx("dcpomatic2"));

		if (!locale->IsOk()) {
			delete locale;
			locale = new wxLocale (wxLANGUAGE_ENGLISH);
		}
	}

	if (locale) {
		dcpomatic_setup_gettext_i18n (wx_to_std (locale->GetCanonicalName ()));
	}
}

#endif


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
context_translation(char const* s)
{
	auto wx_s = char_to_wx(s);
	auto t = wxGetTranslation(wx_s);
	if (t == wx_s) {
		/* No translation; strip the context */
		int c = t.Find(char_to_wx("|"));
		if (c != wxNOT_FOUND) {
			t = t.Mid (c + 1);
		}
	}

	return t;
}


wxString
time_to_timecode (DCPTime t, double fps)
{
	auto w = t.seconds ();
	int const h = (w / 3600);
	w -= h * 3600;
	int const m = (w / 60);
	w -= m * 60;
	int const s = floor (w);
	w -= s;
	int const f = lrint (w * fps);
	return wxString::Format(char_to_wx("%02d:%02d:%02d.%02d"), h, m, s, f);
}


void
setup_audio_channels_choice (wxChoice* choice, int minimum)
{
	vector<pair<string, string>> items;
	for (int i = minimum; i <= 16; i += 2) {
		if (i == 2) {
			items.push_back (make_pair(wx_to_std(_("2 - stereo")), locale_convert<string>(i)));
		} else if (i == 4) {
			items.push_back (make_pair(wx_to_std(_("4 - L/C/R/Lfe")), locale_convert<string>(i)));
		} else if (i == 6) {
			items.push_back (make_pair(wx_to_std(_("6 - 5.1")), locale_convert<string>(i)));
		} else if (i == 8) {
			items.push_back (make_pair(wx_to_std(_("8 - 5.1/HI/VI")), locale_convert<string>(i)));
		} else if (i == 12) {
			items.push_back (make_pair(wx_to_std(_("12 - 7.1/HI/VI")), locale_convert<string>(i)));
		} else {
			items.push_back (make_pair(locale_convert<string> (i), locale_convert<string>(i)));
		}
	}

	checked_set (choice, items);
}


wxSplashScreen*
maybe_show_splash ()
{
	wxSplashScreen* splash = nullptr;

	try {
		wxBitmap bitmap;
		if (bitmap.LoadFile(bitmap_path("splash.png"), wxBITMAP_TYPE_PNG)) {
			{
				/* This wxMemoryDC must be destroyed before bitmap can be used elsewhere */
				wxMemoryDC dc(bitmap);
				auto const version = wxString::Format(char_to_wx("%s (%s)"), std_to_wx(dcpomatic_version), std_to_wx(dcpomatic_git_commit));
				auto screen_size = dc.GetSize();
				auto text_size = dc.GetTextExtent(version);
				dc.DrawText(version, (screen_size.GetWidth() - text_size.GetWidth()) / 2, 236);
			}
#ifdef DCPOMATIC_WINDOWS
			/* Having wxSTAY_ON_TOP means error dialogues hide behind the splash screen on Windows, no matter what I try */
			splash = new wxSplashScreen(bitmap, wxSPLASH_CENTRE_ON_SCREEN | wxSPLASH_NO_TIMEOUT, 0, nullptr, -1, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE | wxFRAME_NO_TASKBAR);
#else
			splash = new wxSplashScreen(bitmap, wxSPLASH_CENTRE_ON_SCREEN | wxSPLASH_NO_TIMEOUT, 0, nullptr, -1);
#endif
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
	auto jm = JobManager::instance ();

	wxProgressDialog progress (title, task, 100, 0, wxPD_CAN_ABORT);

	bool ok = true;

	while (jm->work_to_do()) {
		dcpomatic_sleep_seconds (1);
		if (!progress.Pulse()) {
			/* user pressed cancel */
			for (auto i: jm->get()) {
				i->cancel();
			}
			ok = false;
			break;
		}
	}

	return ok;
}


int
get_offsets (vector<Offset>& offsets)
{
	offsets.push_back({_("UTC-11"),  dcp::UTCOffset(-11,   0)});
	offsets.push_back({_("UTC-10"),  dcp::UTCOffset(-10,   0)});
	offsets.push_back({_("UTC-9"),   dcp::UTCOffset( -9,   0)});
	offsets.push_back({_("UTC-8"),   dcp::UTCOffset( -8,   0)});
	offsets.push_back({_("UTC-7"),   dcp::UTCOffset( -7,   0)});
	offsets.push_back({_("UTC-6"),   dcp::UTCOffset( -6,   0)});
	offsets.push_back({_("UTC-5"),   dcp::UTCOffset( -5,   0)});
	offsets.push_back({_("UTC-4:30"),dcp::UTCOffset( -4, -30)});
	offsets.push_back({_("UTC-4"),   dcp::UTCOffset( -4,   0)});
	offsets.push_back({_("UTC-3:30"),dcp::UTCOffset( -3, -30)});
	offsets.push_back({_("UTC-3"),   dcp::UTCOffset( -3,   0)});
	offsets.push_back({_("UTC-2"),   dcp::UTCOffset( -2,   0)});
	offsets.push_back({_("UTC-1"),   dcp::UTCOffset( -1,   0)});
	int utc = offsets.size();
	offsets.push_back({_("UTC")  ,   dcp::UTCOffset(  0,   0)});
	offsets.push_back({_("UTC+1"),   dcp::UTCOffset(  1,   0)});
	offsets.push_back({_("UTC+2"),   dcp::UTCOffset(  2,   0)});
	offsets.push_back({_("UTC+3"),   dcp::UTCOffset(  3,   0)});
	offsets.push_back({_("UTC+4"),   dcp::UTCOffset(  4,   0)});
	offsets.push_back({_("UTC+5"),   dcp::UTCOffset(  5,   0)});
	offsets.push_back({_("UTC+5:30"),dcp::UTCOffset(  5,  30)});
	/* Nepal Standard Time */
	offsets.push_back({_("UTC+5:45"),dcp::UTCOffset(  5,  45)});
	offsets.push_back({_("UTC+6"),   dcp::UTCOffset(  6,   0)});
	offsets.push_back({_("UTC+7"),   dcp::UTCOffset(  7,   0)});
	offsets.push_back({_("UTC+8"),   dcp::UTCOffset(  8,   0)});
	offsets.push_back({_("UTC+9"),   dcp::UTCOffset(  9,   0)});
	offsets.push_back({_("UTC+9:30"),dcp::UTCOffset(  9,  30)});
	offsets.push_back({_("UTC+10"),  dcp::UTCOffset( 10,   0)});
	offsets.push_back({_("UTC+11"),  dcp::UTCOffset( 11,   0)});
	offsets.push_back({_("UTC+12"),  dcp::UTCOffset( 12,   0)});

	return utc;
}


wxString
bitmap_path (string name)
{
	boost::filesystem::path base;

#ifdef DCPOMATIC_DEBUG
	/* Hack to allow Linux and OS X to find icons when running from the source tree */
	char* path = getenv ("DCPOMATIC_GRAPHICS");
	if (path) {
		base = path;
	} else {
		base = resources_path();
	}

	if (!boost::filesystem::exists(base / name)) {
		base = path / boost::filesystem::path("osx/preferences");
	}
#else
	base = resources_path();
#endif

	auto p = base / name;
	return std_to_wx (p.string());
}


wxString
icon_path(string name)
{
	return gui_is_dark() ? bitmap_path(String::compose("%1_white.png", name)) : bitmap_path(String::compose("%1_black.png", name));
}


wxSize
small_button_size (wxWindow* parent, wxString text)
{
	wxClientDC dc (parent);
	auto size = dc.GetTextExtent (text);
	size.SetHeight (-1);
	size.IncBy (32, 0);
	return size;
}


bool
gui_is_dark ()
{
#if wxCHECK_VERSION(3, 1, 0)
	auto appearance = wxSystemSettings::GetAppearance();
	return appearance.IsDark();
#else
	return false;
#endif
}


#if wxCHECK_VERSION(3,1,0)
double
dpi_scale_factor (wxWindow* window)
{
	return window->GetDPIScaleFactor();
}
#else
double
dpi_scale_factor (wxWindow*)
{
	return 1;
}
#endif



int
search_ctrl_height ()
{
#ifdef __WXGTK3__
	return 30;
#else
	return -1;
#endif
}


void
report_config_load_failure(wxWindow* parent, Config::LoadFailure what)
{
	switch (what) {
	case Config::LoadFailure::CONFIG:
		message_dialog(parent, _("The existing configuration failed to load.  Default values will be used instead.  These may take a short time to create."));
		break;
	}
}


wxString
dcpomatic::wx::report_problem()
{
	return std_to_wx(::report_problem());
}


bool
layout_for_short_screen(wxWindow* reference)
{
	if (Config::instance()->layout_for_short_screen()) {
		return true;
	}

	auto const sn = wxDisplay::GetFromWindow(reference);
	return sn >= 0 && wxDisplay(sn).GetClientArea().height <= 800;
}

