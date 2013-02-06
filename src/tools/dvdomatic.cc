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
#include <boost/filesystem.hpp>
#include <wx/aboutdlg.h>
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
#include "lib/film.h"
#include "lib/format.h"
#include "lib/config.h"
#include "lib/filter.h"
#include "lib/util.h"
#include "lib/scaler.h"
#include "lib/exceptions.h"
#include "lib/version.h"
#include "lib/ui_signaller.h"
#include "lib/log.h"

using std::cout;
using std::string;
using std::stringstream;
using std::map;
using std::make_pair;
using boost::shared_ptr;

static FilmEditor* film_editor = 0;
static FilmViewer* film_viewer = 0;
static shared_ptr<Film> film;
static std::string log_level;
static std::string film_to_load;

static void set_menu_sensitivity ();

class FilmChangedDialog
{
public:
	FilmChangedDialog ()
	{
		stringstream s;
		s << "Save changes to film \"" << film->name() << "\" before closing?";
		_dialog = new wxMessageDialog (0, std_to_wx (s.str()), _("Film changed"), wxYES_NO | wxYES_DEFAULT | wxICON_QUESTION);
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
add_item (wxMenu* menu, std::string text, int id, Sensitivity sens)
{
	wxMenuItem* item = menu->Append (id, std_to_wx (text));
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
	ID_file_quit,
	ID_edit_preferences,
	ID_jobs_make_dcp,
	ID_jobs_send_dcp_to_tms,
	ID_jobs_examine_content,
	ID_jobs_make_dcp_from_existing_transcode,
	ID_help_about
};

void
setup_menu (wxMenuBar* m)
{
	wxMenu* file = new wxMenu;
	add_item (file, "New...", ID_file_new, ALWAYS);
	add_item (file, "&Open...", ID_file_open, ALWAYS);
	file->AppendSeparator ();
	add_item (file, "&Save", ID_file_save, NEEDS_FILM);
	file->AppendSeparator ();
	add_item (file, "&Properties...", ID_file_properties, NEEDS_FILM);
	file->AppendSeparator ();
	add_item (file, "&Quit", ID_file_quit, ALWAYS);

	wxMenu* edit = new wxMenu;
	add_item (edit, "&Preferences...", ID_edit_preferences, ALWAYS);

	wxMenu* jobs = new wxMenu;
	add_item (jobs, "&Make DCP", ID_jobs_make_dcp, NEEDS_FILM);
	add_item (jobs, "&Send DCP to TMS", ID_jobs_send_dcp_to_tms, NEEDS_FILM);
	jobs->AppendSeparator ();
	add_item (jobs, "&Examine content", ID_jobs_examine_content, NEEDS_FILM);
	add_item (jobs, "Make DCP from existing &transcode", ID_jobs_make_dcp_from_existing_transcode, NEEDS_FILM);

	wxMenu* help = new wxMenu;
	add_item (help, "About", ID_help_about, ALWAYS);

	m->Append (file, _("&File"));
	m->Append (edit, _("&Edit"));
	m->Append (jobs, _("&Jobs"));
	m->Append (help, _("&Help"));
}

bool
window_closed (wxCommandEvent &)
{
	maybe_save_then_delete_film ();
	return false;
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
		Connect (ID_file_quit, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::file_quit));
		Connect (ID_edit_preferences, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::edit_preferences));
		Connect (ID_jobs_make_dcp, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::jobs_make_dcp));
		Connect (ID_jobs_send_dcp_to_tms, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::jobs_send_dcp_to_tms));
		Connect (ID_jobs_examine_content, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::jobs_examine_content));
		Connect (ID_jobs_make_dcp_from_existing_transcode, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::jobs_make_dcp_from_existing_transcode));
		Connect (ID_help_about, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler (Frame::help_about));

		wxPanel* panel = new wxPanel (this);
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		s->Add (panel, 1, wxEXPAND);
		SetSizer (s);

		film_editor = new FilmEditor (film, panel);
		film_viewer = new FilmViewer (film, panel);
		JobManagerView* job_manager_view = new JobManagerView (panel);

		wxSizer* rhs_sizer = new wxBoxSizer (wxVERTICAL);
		rhs_sizer->Add (film_viewer, 3, wxEXPAND | wxALL);
		rhs_sizer->Add (job_manager_view, 1, wxEXPAND | wxALL);

		wxBoxSizer* main_sizer = new wxBoxSizer (wxHORIZONTAL);
		main_sizer->Add (film_editor, 0, wxALL, 6);
		main_sizer->Add (rhs_sizer, 1, wxEXPAND | wxALL, 6);
		panel->SetSizer (main_sizer);

		set_menu_sensitivity ();

		/* XXX: calling these here is a bit of a hack */
		film_editor->setup_visibility ();
		
		film_editor->FileChanged.connect (bind (&Frame::file_changed, this, _1));
		if (film) {
			file_changed (film->directory ());
		} else {
			file_changed ("");
		}
		
		set_film ();
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
		s << "DVD-o-matic";
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

			if (boost::filesystem::exists (d->get_path())) {
				error_dialog (this, wxString::Format (_("The directory %s already exists"), d->get_path().c_str()));
				return;
			}
			
			maybe_save_then_delete_film ();
			film.reset (new Film (d->get_path (), false));
			film->log()->set_level (log_level);
			film->set_name (boost::filesystem::path (d->get_path()).filename().generic_string());
			set_film ();
		}
		
		d->Destroy ();
	}

	void file_open (wxCommandEvent &)
	{
		wxDirDialog* c = new wxDirDialog (this, _("Select film to open"), wxStandardPaths::Get().GetDocumentsDir(), wxDEFAULT_DIALOG_STYLE | wxDD_DIR_MUST_EXIST);
		int const r = c->ShowModal ();
		
		if (r == wxID_OK) {
			maybe_save_then_delete_film ();
			try {
				film.reset (new Film (wx_to_std (c->GetPath ())));
				film->log()->set_level (log_level);
				set_film ();
			} catch (std::exception& e) {
				wxString p = c->GetPath ();
				wxCharBuffer b = p.ToUTF8 ();
				error_dialog (this, wxString::Format (_("Could not open film at %s (%s)"), p.data(), e.what()));
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
	
	void file_quit (wxCommandEvent &)
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
		JobWrapper::make_dcp (this, film, true);
	}
	
	void jobs_make_dcp_from_existing_transcode (wxCommandEvent &)
	{
		JobWrapper::make_dcp (this, film, false);
	}
	
	void jobs_send_dcp_to_tms (wxCommandEvent &)
	{
		film->send_dcp_to_tms ();
	}
	
	void jobs_examine_content (wxCommandEvent &)
	{
		film->examine_content ();
	}
	
	void help_about (wxCommandEvent &)
	{
		wxAboutDialogInfo info;
		info.SetName (_("DVD-o-matic"));
		if (strcmp (dvdomatic_git_commit, "release") == 0) {
			info.SetVersion (std_to_wx (String::compose ("version %1", dvdomatic_version)));
		} else {
			info.SetVersion (std_to_wx (String::compose ("version %1 git %2", dvdomatic_version, dvdomatic_git_commit)));
		}
		info.SetDescription (_("Free, open-source DCP generation from almost anything."));
		info.SetCopyright (_("(C) Carl Hetherington, Terrence Meiczinger, Paul Davis, Ole Laursen"));
		wxArrayString authors;
		authors.Add (wxT ("Carl Hetherington"));
		authors.Add (wxT ("Terrence Meiczinger"));
		authors.Add (wxT ("Paul Davis"));
		authors.Add (wxT ("Ole Laursen"));
		info.SetDevelopers (authors);
		info.SetWebSite (wxT ("http://carlh.net/software/dvdomatic"));
		wxAboutBox (info);
	}
};

#if wxMINOR_VERSION == 9
static const wxCmdLineEntryDesc command_line_description[] = {
	{ wxCMD_LINE_OPTION, "l", "log", "set log level (silent, verbose or timing)", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
        { wxCMD_LINE_PARAM, 0, 0, "film to load", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_MULTIPLE | wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_NONE, "", "", "", wxCmdLineParamType (0), 0 }
};
#else
static const wxCmdLineEntryDesc command_line_description[] = {
	{ wxCMD_LINE_OPTION, wxT("l"), wxT("log"), wxT("set log level (silent, verbose or timing)"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
        { wxCMD_LINE_PARAM, 0, 0, wxT("film to load"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_MULTIPLE | wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_NONE, wxT(""), wxT(""), wxT(""), wxCmdLineParamType (0), 0 }
};
#endif

class App : public wxApp
{
	bool OnInit ()
	{
		if (!wxApp::OnInit()) {
			return false;
		}
		
#ifdef DVDOMATIC_POSIX		
		unsetenv ("UBUNTU_MENUPROXY");
#endif		
		
		wxInitAllImageHandlers ();
		
		dvdomatic_setup ();

		if (!film_to_load.empty() && boost::filesystem::is_directory (film_to_load)) {
			film.reset (new Film (film_to_load));
			film->log()->set_level (log_level);
		}

		Frame* f = new Frame (_("DVD-o-matic"));
		SetTopWindow (f);
		f->Maximize ();
		f->Show ();

		ui_signaller = new wxUISignaller (this);
		this->Connect (-1, wxEVT_IDLE, wxIdleEventHandler (App::idle));

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
			film_to_load = wx_to_std (parser.GetParam(0));
		}

		wxString log;
		if (parser.Found(wxT("log"), &log)) {
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
