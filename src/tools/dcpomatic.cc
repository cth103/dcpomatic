/*
    Copyright (C) 2012-2013 Carl Hetherington <cth@carlh.net>

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
#include "wx/kdm_dialog.h"
#include "wx/servers_list_dialog.h"
#include "wx/hints_dialog.h"
#include "lib/film.h"
#include "lib/config.h"
#include "lib/util.h"
#include "lib/version.h"
#include "lib/ui_signaller.h"
#include "lib/log.h"
#include "lib/job_manager.h"
#include "lib/transcode_job.h"
#include "lib/exceptions.h"
#include "lib/cinema.h"
#include "lib/kdm.h"
#include "lib/send_kdm_email_job.h"

using std::cout;
using std::string;
using std::wstring;
using std::stringstream;
using std::map;
using std::make_pair;
using std::list;
using std::exception;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

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

#define ALWAYS                  0x0
#define NEEDS_FILM              0x1
#define NOT_DURING_DCP_CREATION 0x2
#define NEEDS_DCP               0x4

map<wxMenuItem*, int> menu_items;
	
void
add_item (wxMenu* menu, wxString text, int id, int sens)
{
	wxMenuItem* item = menu->Append (id, text);
	menu_items.insert (make_pair (item, sens));
}

void
set_menu_sensitivity ()
{
	list<shared_ptr<Job> > jobs = JobManager::instance()->get ();
	list<shared_ptr<Job> >::iterator i = jobs.begin();
	while (i != jobs.end() && dynamic_pointer_cast<TranscodeJob> (*i) == 0) {
		++i;
	}
	bool const dcp_creation = (i != jobs.end ()) && !(*i)->finished ();
	bool const have_dcp = film && !film->dcps().empty ();

	for (map<wxMenuItem*, int>::iterator j = menu_items.begin(); j != menu_items.end(); ++j) {

		bool enabled = true;

		if ((j->second & NEEDS_FILM) && film == 0) {
			enabled = false;
		}

		if ((j->second & NOT_DURING_DCP_CREATION) && dcp_creation) {
			enabled = false;
		}

		if ((j->second & NEEDS_DCP) && !have_dcp) {
			enabled = false;
		}
		
		j->first->Enable (enabled);
	}
}

enum {
	ID_file_new = 1,
	ID_file_open,
	ID_file_save,
	ID_file_properties,
	ID_jobs_make_dcp,
	ID_jobs_make_kdms,
	ID_jobs_send_dcp_to_tms,
	ID_jobs_show_dcp,
	ID_tools_hints,
	ID_tools_encoding_servers,
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
	add_item (jobs_menu, _("&Make DCP"), ID_jobs_make_dcp, NEEDS_FILM | NOT_DURING_DCP_CREATION);
	add_item (jobs_menu, _("Make &KDMs..."), ID_jobs_make_kdms, NEEDS_FILM | NEEDS_DCP);
	add_item (jobs_menu, _("&Send DCP to TMS"), ID_jobs_send_dcp_to_tms, NEEDS_FILM | NOT_DURING_DCP_CREATION | NEEDS_DCP);
	add_item (jobs_menu, _("S&how DCP"), ID_jobs_show_dcp, NEEDS_FILM | NOT_DURING_DCP_CREATION | NEEDS_DCP);

	wxMenu* tools = new wxMenu;
	add_item (tools, _("Hints..."), ID_tools_hints, 0);
	add_item (tools, _("Encoding Servers..."), ID_tools_encoding_servers, 0);

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
	m->Append (tools, _("&Tools"));
	m->Append (help, _("&Help"));
}

class Frame : public wxFrame
{
public:
	Frame (wxString const & title)
		: wxFrame (NULL, -1, title)
		, _hints_dialog (0)
		, _servers_list_dialog (0)
	{
#ifdef DCPOMATIC_WINDOWS_CONSOLE		
                AllocConsole();
		
		HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
		int hCrt = _open_osfhandle((intptr_t) handle_out, _O_TEXT);
		FILE* hf_out = _fdopen(hCrt, "w");
		setvbuf(hf_out, NULL, _IONBF, 1);
		*stdout = *hf_out;
		
		HANDLE handle_in = GetStdHandle(STD_INPUT_HANDLE);
		hCrt = _open_osfhandle((intptr_t) handle_in, _O_TEXT);
		FILE* hf_in = _fdopen(hCrt, "r");
		setvbuf(hf_in, NULL, _IONBF, 128);
		*stdin = *hf_in;
#endif

		wxMenuBar* bar = new wxMenuBar;
		setup_menu (bar);
		SetMenuBar (bar);

		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::file_new, this),               ID_file_new);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::file_open, this),              ID_file_open);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::file_save, this),              ID_file_save);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::file_properties, this),        ID_file_properties);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::file_exit, this),              wxID_EXIT);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::edit_preferences, this),       wxID_PREFERENCES);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::jobs_make_dcp, this),          ID_jobs_make_dcp);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::jobs_make_kdms, this),         ID_jobs_make_kdms);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::jobs_send_dcp_to_tms, this),   ID_jobs_send_dcp_to_tms);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::jobs_show_dcp, this),          ID_jobs_show_dcp);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::tools_hints, this),            ID_tools_hints);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::tools_encoding_servers, this), ID_tools_encoding_servers);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::help_about, this),             wxID_ABOUT);

		Bind (wxEVT_CLOSE_WINDOW, boost::bind (&Frame::close, this, _1));

		/* Use a panel as the only child of the Frame so that we avoid
		   the dark-grey background on Windows.
		*/
		wxPanel* overall_panel = new wxPanel (this, wxID_ANY);

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

		JobManager::instance()->ActiveJobsChanged.connect (boost::bind (set_menu_sensitivity));

		set_film ();
		overall_panel->SetSizer (main_sizer);
	}

private:

	void set_film ()
	{
		film_viewer->set_film (film);
		film_editor->set_film (film);
		set_menu_sensitivity ();
	}

	void file_changed (boost::filesystem::path f)
	{
		stringstream s;
		s << wx_to_std (_("DCP-o-matic"));
		if (!f.empty ()) {
			s << " - " << f.string ();
		}
		
		SetTitle (std_to_wx (s.str()));
	}
	
	void file_new ()
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
								     d->get_path().string().c_str())
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

	void file_open ()
	{
		wxDirDialog* c = new wxDirDialog (
			this,
			_("Select film to open"),
			std_to_wx (Config::instance()->default_directory_or (wx_to_std (wxStandardPaths::Get().GetDocumentsDir())).string ()),
			wxDEFAULT_DIALOG_STYLE | wxDD_DIR_MUST_EXIST
			);
		
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

	void file_save ()
	{
		film->write_metadata ();
	}

	void file_properties ()
	{
		PropertiesDialog* d = new PropertiesDialog (this, film);
		d->ShowModal ();
		d->Destroy ();
	}
	
	void file_exit ()
	{
		if (!should_close ()) {
			return;
		}
		
		maybe_save_then_delete_film ();
		Close (true);
	}

	void edit_preferences ()
	{
		ConfigDialog* d = new ConfigDialog (this);
		d->ShowModal ();
		d->Destroy ();
		Config::instance()->write ();
	}

	void jobs_make_dcp ()
	{
		JobWrapper::make_dcp (this, film);
	}

	void jobs_make_kdms ()
	{
		if (!film) {
			return;
		}
		
		KDMDialog* d = new KDMDialog (this, film);
		if (d->ShowModal () != wxID_OK) {
			d->Destroy ();
			return;
		}

		try {
			if (d->write_to ()) {
				write_kdm_files (film, d->screens (), d->dcp (), d->from (), d->until (), d->directory ());
			} else {
				JobManager::instance()->add (
					shared_ptr<Job> (new SendKDMEmailJob (film, d->screens (), d->dcp (), d->from (), d->until ()))
					);
			}
		} catch (KDMError& e) {
			error_dialog (this, e.what ());
		}
	
		d->Destroy ();
	}
	
	void jobs_send_dcp_to_tms ()
	{
		film->send_dcp_to_tms ();
	}

	void jobs_show_dcp ()
	{
#ifdef __WXMSW__
		string d = film->directory().string ();
		wstring w;
		w.assign (d.begin(), d.end());
		ShellExecute (0, L"open", w.c_str(), 0, 0, SW_SHOWDEFAULT);
#else
		int r = system ("which nautilus");
		if (WEXITSTATUS (r) == 0) {
			r = system (string ("nautilus " + film->directory().string()).c_str ());
			if (WEXITSTATUS (r)) {
				error_dialog (this, _("Could not show DCP (could not run nautilus)"));
			}
		} else {
			int r = system ("which konqueror");
			if (WEXITSTATUS (r) == 0) {
				r = system (string ("konqueror " + film->directory().string()).c_str ());
				if (WEXITSTATUS (r)) {
					error_dialog (this, _("Could not show DCP (could not run konqueror)"));
				}
			}
		}
#endif		
	}

	void tools_hints ()
	{
		if (!_hints_dialog) {
			_hints_dialog = new HintsDialog (this, film);
		}

		_hints_dialog->Show ();
	}

	void tools_encoding_servers ()
	{
		if (!_servers_list_dialog) {
			_servers_list_dialog = new ServersListDialog (this);
		}

		_servers_list_dialog->Show ();
	}

	void help_about ()
	{
		AboutDialog* d = new AboutDialog (this);
		d->ShowModal ();
		d->Destroy ();
	}

	bool should_close ()
	{
		if (!JobManager::instance()->work_to_do ()) {
			return true;
		}

		wxMessageDialog* d = new wxMessageDialog (
			0,
			_("There are unfinished jobs; are you sure you want to quit?"),
			_("Unfinished jobs"),
			wxYES_NO | wxYES_DEFAULT | wxICON_QUESTION
			);

		bool const r = d->ShowModal() == wxID_YES;
		d->Destroy ();
		return r;
	}
		
	void close (wxCloseEvent& ev)
	{
		if (!should_close ()) {
			ev.Veto ();
			return;
		}

		maybe_save_then_delete_film ();

		ev.Skip ();
	}

	HintsDialog* _hints_dialog;
	ServersListDialog* _servers_list_dialog;
};

static const wxCmdLineEntryDesc command_line_description[] = {
	{ wxCMD_LINE_OPTION, "l", "log", "set log level (silent, verbose or timing)", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_SWITCH, "n", "new", "create new film", wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_PARAM, 0, 0, "film to load or create", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_MULTIPLE | wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_NONE, "", "", "", wxCmdLineParamType (0), 0 }
};

class App : public wxApp
{
	bool OnInit ()
	try
	{
		SetAppName (_("DCP-o-matic"));
		
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
		this->Bind (wxEVT_IDLE, boost::bind (&App::idle, this));

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

	void idle ()
	{
		ui_signaller->ui_idle ();
	}
};

IMPLEMENT_APP (App)
