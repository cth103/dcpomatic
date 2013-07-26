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

#include <iostream>
#include <fstream>
#include <boost/filesystem.hpp>
#ifdef __WXMSW__
#include <shellapi.h>
#endif
#ifdef __WXOSX__
#include <ApplicationServices/ApplicationServices.h>
#endif
#include <wx/generic/aboutdlgg.h>
#include <wx/stdpaths.h>
#include <wx/cmdline.h>
#include "wx/film_viewer.h"
#include "wx/film_editor.h"
#include "wx/job_manager_view.h"
#include "wx/config_dialog.h"
#include "wx/job_wrapper.h"
#include "wx/wx_util.h"
#include "wx/new_film_dialog.h"
#include "wx/properties_dialog.h"
#include "wx/wx_ui_signaller.h"
#include "wx/about_dialog.h"
#include "lib/film.h"
#include "lib/config.h"
#include "lib/util.h"
#include "lib/version.h"
#include "lib/ui_signaller.h"
#include "lib/log.h"

using std::cout;
using std::string;
using std::wstring;
using std::stringstream;
using std::map;
using std::make_pair;
using std::exception;
using std::ofstream;
using boost::shared_ptr;

static FilmEditor* film_editor = 0;
static FilmViewer* film_viewer = 0;
static shared_ptr<Film> film;
static std::string log_level;
static std::string film_to_load;
static std::string film_to_create;
static wxMenu* jobs_menu = 0;

static void set_menu_sensitivity ();

class FilmChangedDialog
{
public:
	FilmChangedDialog ()
	{
		_dialog = new wxMessageDialog (
			0,
			wxString::Format (_("Save changes to film \"%s\" before closing?"), std_to_wx (film->name ()).data()),
			_("Film changed"),
			wxYES_NO | wxYES_DEFAULT | wxICON_QUESTION
			);
	}

	~FilmChangedDialog ()
	{
		_dialog->Destroy ();
	}

	int run ()
	{
		return _dialog->ShowModal ();
	}

private:
	/* Not defined */
	FilmChangedDialog (FilmChangedDialog const &);
	
	wxMessageDialog* _dialog;
};


void
maybe_save_then_delete_film ()
{
	if (!film) {
		return;
	}
			
	if (film->dirty ()) {
		FilmChangedDialog d;
		switch (d.run ()) {
		case wxID_NO:
			break;
		case wxID_YES:
			film->write_metadata ();
			break;
		}
	}
	
	film.reset ();
}

enum Sensitivity {
	ALWAYS,
	NEEDS_FILM
};

map<wxMenuItem*, Sensitivity> menu_items;
	
void
add_item (wxMenu* menu, wxString text, int id, Sensitivity sens)
{
	wxMenuItem* item = menu->Append (id, text);
	menu_items.insert (make_pair (item, sens));
}

void
set_menu_sensitivity ()
{
	for (map<wxMenuItem*, Sensitivity>::iterator i = menu_items.begin(); i != menu_items.end(); ++i) {
		if (i->second == NEEDS_FILM) {
			i->first->Enable (film != 0);
		} else {
			i->first->Enable (true);
		}
	}
}

enum {
	ID_file_new = 1,
	ID_file_open,
	ID_file_save,
	ID_file_properties,
	ID_jobs_make_dcp,
	ID_jobs_send_dcp_to_tms,
	ID_jobs_show_dcp,
};

void
setup_menu (wxMenuBar* m)
{
	wxMenu* file = new wxMenu;
	add_item (file, _("New..."), ID_file_new, ALWAYS);
	add_item (file, _("&Open..."), ID_file_open, ALWAYS);
	file->AppendSeparator ();
	add_item (file, _("&Save"), ID_file_save, NEEDS_FILM);
	file->AppendSeparator ();
	add_item (file, _("&Properties..."), ID_file_properties, NEEDS_FILM);
#ifndef __WXOSX__	
	file->AppendSeparator ();
#endif

#ifdef __WXOSX__	
	add_item (file, _("&Exit"), wxID_EXIT, ALWAYS);
#else
	add_item (file, _("&Quit"), wxID_EXIT, ALWAYS);
#endif	
	

#ifdef __WXOSX__	
	add_item (file, _("&Preferences..."), wxID_PREFERENCES, ALWAYS);
#else
	wxMenu* edit = new wxMenu;
	add_item (edit, _("&Preferences..."), wxID_PREFERENCES, ALWAYS);
#endif	

	jobs_menu = new wxMenu;
	add_item (jobs_menu, _("&Make DCP"), ID_jobs_make_dcp, NEEDS_FILM);
	add_item (jobs_menu, _("&Send DCP to TMS"), ID_jobs_send_dcp_to_tms, NEEDS_FILM);
	add_item (jobs_menu, _("S&how DCP"), ID_jobs_show_dcp, NEEDS_FILM);

	wxMenu* help = new wxMenu;
#ifdef __WXOSX__	
	add_item (help, _("About DCP-o-matic"), wxID_ABOUT, ALWAYS);
#else	
	add_item (help, _("About"), wxID_ABOUT, ALWAYS);
#endif	

	m->Append (file, _("&File"));
#ifndef __WXOSX__	
	m->Append (edit, _("&Edit"));
#endif	
	m->Append (jobs_menu, _("&Jobs"));
	m->Append (help, _("&Help"));
}

class Frame : public wxFrame
{
public:
	Frame (wxString const & title)
		: wxFrame (NULL, -1, title)
	{
		wxMenuBar* bar = new wxMenuBar;
		setup_menu (bar);
		SetMenuBar (bar);

		Connect (ID_file_new, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::file_new));
		Connect (ID_file_open, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::file_open));
		Connect (ID_file_save, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::file_save));
		Connect (ID_file_properties, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::file_properties));
		Connect (wxID_EXIT, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::file_exit));
		Connect (wxID_PREFERENCES, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::edit_preferences));
		Connect (ID_jobs_make_dcp, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::jobs_make_dcp));
		Connect (ID_jobs_send_dcp_to_tms, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::jobs_send_dcp_to_tms));
		Connect (ID_jobs_show_dcp, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::jobs_show_dcp));
		Connect (wxID_ABOUT, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::help_about));
		Connect (wxID_ANY, wxEVT_MENU_OPEN, wxMenuEventHandler (Frame::menu_opened));

		/* Use a panel as the only child of the Frame so that we avoid
		   the dark-grey background on Windows.
		*/
		wxPanel* overall_panel = new wxPanel (this, wxID_ANY);
		wxBoxSizer* overall_sizer = new wxBoxSizer (wxEXPAND);
		overall_panel->SetSizer (overall_sizer);

		film_editor = new FilmEditor (film, overall_panel);
		film_viewer = new FilmViewer (film, overall_panel);
		JobManagerView* job_manager_view = new JobManagerView (overall_panel, static_cast<JobManagerView::Buttons> (0));

		wxBoxSizer* right_sizer = new wxBoxSizer (wxVERTICAL);
		right_sizer->Add (film_viewer, 2, wxEXPAND | wxALL, 6);
		right_sizer->Add (job_manager_view, 1, wxEXPAND | wxALL, 6);

		wxBoxSizer* main_sizer = new wxBoxSizer (wxHORIZONTAL);
		main_sizer->Add (film_editor, 1, wxEXPAND | wxALL, 6);
		main_sizer->Add (right_sizer, 2, wxEXPAND | wxALL, 6);

		set_menu_sensitivity ();

		film_editor->FileChanged.connect (bind (&Frame::file_changed, this, _1));
		if (film) {
			file_changed (film->directory ());
		} else {
			file_changed ("");
		}

		set_film ();
		SetSizer (main_sizer);
	}

private:

	void menu_opened (wxMenuEvent& ev)
	{
		if (ev.GetMenu() != jobs_menu) {
			return;
		}

		bool const have_dcp = film && film->have_dcp();
		jobs_menu->Enable (ID_jobs_send_dcp_to_tms, have_dcp);
		jobs_menu->Enable (ID_jobs_show_dcp, have_dcp);
	}

	void set_film ()
	{
		film_viewer->set_film (film);
		film_editor->set_film (film);
		set_menu_sensitivity ();
	}

	void file_changed (string f)
	{
		stringstream s;
		s << wx_to_std (_("DCP-o-matic"));
		if (!f.empty ()) {
			s << " - " << f;
		}
		
		SetTitle (std_to_wx (s.str()));
	}
	
	void file_new (wxCommandEvent &)
	{
		NewFilmDialog* d = new NewFilmDialog (this);
		int const r = d->ShowModal ();
		
		if (r == wxID_OK) {

			if (boost::filesystem::is_directory (d->get_path()) && !boost::filesystem::is_empty(d->get_path())) {
				if (!confirm_dialog (
					    this,
					    std_to_wx (
						    String::compose (wx_to_std (_("The directory %1 already exists and is not empty.  "
										  "Are you sure you want to use it?")),
								     d->get_path().c_str())
						    )
					    )) {
					return;
				}
			} else if (boost::filesystem::is_regular_file (d->get_path())) {
				error_dialog (
					this,
					String::compose (wx_to_std (_("%1 already exists as a file, so you cannot use it for a new film.")), d->get_path().c_str())
					);
				return;
			}
			
			maybe_save_then_delete_film ();
			film.reset (new Film (d->get_path ()));
			film->write_metadata ();
			film->log()->set_level (log_level);
			film->set_name (boost::filesystem::path (d->get_path()).filename().generic_string());
			set_film ();
		}
		
		d->Destroy ();
	}

	void file_open (wxCommandEvent &)
	{
		wxDirDialog* c = new wxDirDialog (this, _("Select film to open"), wxStandardPaths::Get().GetDocumentsDir(), wxDEFAULT_DIALOG_STYLE | wxDD_DIR_MUST_EXIST);
		int r;
		while (1) {
			r = c->ShowModal ();
			if (r == wxID_OK && c->GetPath() == wxStandardPaths::Get().GetDocumentsDir()) {
				error_dialog (this, _("You did not select a folder.  Make sure that you select a folder before clicking Open."));
			} else {
				break;
			}
		}
			
		if (r == wxID_OK) {
			maybe_save_then_delete_film ();
			try {
				film.reset (new Film (wx_to_std (c->GetPath ())));
				film->read_metadata ();
				film->log()->set_level (log_level);
				set_film ();
			} catch (std::exception& e) {
				wxString p = c->GetPath ();
				wxCharBuffer b = p.ToUTF8 ();
				error_dialog (this, wxString::Format (_("Could not open film at %s (%s)"), p.data(), std_to_wx (e.what()).data()));
			}
		}

		c->Destroy ();
	}

	void file_save (wxCommandEvent &)
	{
		film->write_metadata ();
	}

	void file_properties (wxCommandEvent &)
	{
		PropertiesDialog* d = new PropertiesDialog (this, film);
		d->ShowModal ();
		d->Destroy ();
	}
	
	void file_exit (wxCommandEvent &)
	{
		maybe_save_then_delete_film ();
		Close (true);
	}

	void edit_preferences (wxCommandEvent &)
	{
		ConfigDialog* d = new ConfigDialog (this);
		d->ShowModal ();
		d->Destroy ();
		Config::instance()->write ();
	}

	void jobs_make_dcp (wxCommandEvent &)
	{
		JobWrapper::make_dcp (this, film);
	}
	
	void jobs_send_dcp_to_tms (wxCommandEvent &)
	{
		film->send_dcp_to_tms ();
	}

	void jobs_show_dcp (wxCommandEvent &)
	{
#ifdef __WXMSW__
		string d = film->directory();
		wstring w;
		w.assign (d.begin(), d.end());
		ShellExecute (0, L"open", w.c_str(), 0, 0, SW_SHOWDEFAULT);
#else
		int r = system ("which nautilus");
		if (WEXITSTATUS (r) == 0) {
			r = system (string ("nautilus " + film->directory()).c_str ());
			if (WEXITSTATUS (r)) {
				error_dialog (this, _("Could not show DCP (could not run nautilus)"));
			}
		} else {
			int r = system ("which konqueror");
			if (WEXITSTATUS (r) == 0) {
				r = system (string ("konqueror " + film->directory()).c_str ());
				if (WEXITSTATUS (r)) {
					error_dialog (this, _("Could not show DCP (could not run konqueror)"));
				}
			}
		}
#endif		
	}

	void help_about (wxCommandEvent &)
	{
		AboutDialog* d = new AboutDialog (this);
		d->ShowModal ();
		d->Destroy ();
	}
};

#if wxMINOR_VERSION == 9
static const wxCmdLineEntryDesc command_line_description[] = {
	{ wxCMD_LINE_OPTION, "l", "log", "set log level (silent, verbose or timing)", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_SWITCH, "n", "new", "create new film", wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_PARAM, 0, 0, "film to load or create", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_MULTIPLE | wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_NONE, "", "", "", wxCmdLineParamType (0), 0 }
};
#else
static const wxCmdLineEntryDesc command_line_description[] = {
	{ wxCMD_LINE_OPTION, wxT("l"), wxT("log"), wxT("set log level (silent, verbose or timing)"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_SWITCH, wxT("n"), wxT("new"), wxT("create new film"), wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_PARAM, 0, 0, wxT("film to load or create"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_MULTIPLE | wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_NONE, wxT(""), wxT(""), wxT(""), wxCmdLineParamType (0), 0 }
};
#endif

class App : public wxApp
{
	bool OnInit ()
	try
	{
		if (!wxApp::OnInit()) {
			return false;
		}
		
#ifdef DCPOMATIC_LINUX	
		unsetenv ("UBUNTU_MENUPROXY");
#endif

#ifdef __WXOSX__		
		ProcessSerialNumber serial;
		GetCurrentProcess (&serial);
		TransformProcessType (&serial, kProcessTransformToForegroundApplication);
#endif		

		wxInitAllImageHandlers ();

		/* Enable i18n; this will create a Config object
		   to look for a force-configured language.  This Config
		   object will be wrong, however, because dcpomatic_setup
		   hasn't yet been called and there aren't any scalers, filters etc.
		   set up yet.
		*/
		dcpomatic_setup_i18n ();

		/* Set things up, including scalers / filters etc.
		   which will now be internationalised correctly.
		*/
		dcpomatic_setup ();

		/* Force the configuration to be re-loaded correctly next
		   time it is needed.
		*/
		Config::drop ();

		if (!film_to_load.empty() && boost::filesystem::is_directory (film_to_load)) {
			try {
				film.reset (new Film (film_to_load));
				film->read_metadata ();
				film->log()->set_level (log_level);
			} catch (exception& e) {
				error_dialog (0, std_to_wx (String::compose (wx_to_std (_("Could not load film %1 (%2)")), film_to_load, e.what())));
			}
		}

		if (!film_to_create.empty ()) {
			film.reset (new Film (film_to_create));
			film->write_metadata ();
			film->log()->set_level (log_level);
			film->set_name (boost::filesystem::path (film_to_create).filename().generic_string ());
		}

		Frame* f = new Frame (_("DCP-o-matic"));
		SetTopWindow (f);
		f->Maximize ();
		f->Show ();

		ui_signaller = new wxUISignaller (this);
		this->Connect (-1, wxEVT_IDLE, wxIdleEventHandler (App::idle));

		return true;
	}
	catch (exception& e)
	{
		error_dialog (0, wxString::Format ("DCP-o-matic could not start: %s", e.what ()));
		return true;
	}

	void OnInitCmdLine (wxCmdLineParser& parser)
	{
		parser.SetDesc (command_line_description);
		parser.SetSwitchChars (wxT ("-"));
	}

	bool OnCmdLineParsed (wxCmdLineParser& parser)
	{
		if (parser.GetParamCount() > 0) {
			if (parser.Found (wxT ("new"))) {
				film_to_create = wx_to_std (parser.GetParam (0));
			} else {
				film_to_load = wx_to_std (parser.GetParam(0));
			}
		}

		wxString log;
		if (parser.Found (wxT ("log"), &log)) {
			log_level = wx_to_std (log);
		}

		return true;
	}

	void idle (wxIdleEvent &)
	{
		ui_signaller->ui_idle ();
	}
};

IMPLEMENT_APP (App)
