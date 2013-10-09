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
#include <quickmail.h>
#include <zip.h>
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

using std::cout;
using std::string;
using std::wstring;
using std::stringstream;
using std::map;
using std::make_pair;
using std::list;
using std::exception;
using std::ofstream;
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
	bool const dcp_creation = (i != jobs.end ());

	for (map<wxMenuItem*, int>::iterator j = menu_items.begin(); j != menu_items.end(); ++j) {

		bool enabled = true;

		if ((j->second & NEEDS_FILM) && film == 0) {
			enabled = false;
		}

		if ((j->second & NOT_DURING_DCP_CREATION) && dcp_creation) {
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
	add_item (jobs_menu, _("Make &KDMs..."), ID_jobs_make_kdms, NEEDS_FILM);
	add_item (jobs_menu, _("&Send DCP to TMS"), ID_jobs_send_dcp_to_tms, NEEDS_FILM | NOT_DURING_DCP_CREATION);
	add_item (jobs_menu, _("S&how DCP"), ID_jobs_show_dcp, NEEDS_FILM | NOT_DURING_DCP_CREATION);

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

struct ScreenKDM
{
	ScreenKDM (shared_ptr<Screen> s, libdcp::KDM k)
		: screen (s)
		, kdm (k)
	{}
	
	shared_ptr<Screen> screen;
	libdcp::KDM kdm;
};

/* Not complete but sufficient for our purposes (we're using
   ScreenKDM in a list where all the screens will be unique).
*/
bool operator== (ScreenKDM const & a, ScreenKDM const & b)
{
	return a.screen == b.screen;
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

		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::file_new, this),             ID_file_new);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::file_open, this),            ID_file_open);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::file_save, this),            ID_file_save);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::file_properties, this),      ID_file_properties);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::file_exit, this),            wxID_EXIT);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::edit_preferences, this),     wxID_PREFERENCES);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::jobs_make_dcp, this),        ID_jobs_make_dcp);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::jobs_make_kdms, this),       ID_jobs_make_kdms);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::jobs_send_dcp_to_tms, this), ID_jobs_send_dcp_to_tms);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::jobs_show_dcp, this),        ID_jobs_show_dcp);
		Bind (wxEVT_COMMAND_MENU_SELECTED, boost::bind (&Frame::help_about, this),           wxID_ABOUT);

		Bind (wxEVT_MENU_OPEN, boost::bind (&Frame::menu_opened, this, _1));
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

	void menu_opened (wxMenuEvent& ev)
	{
		if (ev.GetMenu() != jobs_menu) {
			return;
		}

		bool const have_dcp = false;//film && film->have_dcp();
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

	void file_open ()
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
		
		KDMDialog* d = new KDMDialog (this);
		if (d->ShowModal () != wxID_OK) {
			d->Destroy ();
			return;
		}
		
		try {
			list<shared_ptr<Screen> > screens = d->screens ();
			list<libdcp::KDM> kdms = film->make_kdms (
				screens,
				d->from (),
				d->until ()
				);

			list<ScreenKDM> screen_kdms;
			
			list<shared_ptr<Screen> >::iterator i = screens.begin ();
			list<libdcp::KDM>::iterator j = kdms.begin ();
			while (i != screens.end() && j != kdms.end ()) {
				screen_kdms.push_back (ScreenKDM (*i, *j));
				++i;
				++j;
			}
			
			if (d->write_to ()) {
				/* Write KDMs to the specified directory */
				for (list<ScreenKDM>::iterator i = screen_kdms.begin(); i != screen_kdms.end(); ++i) {
					boost::filesystem::path out = d->directory ();
					out /= tidy_for_filename (i->screen->cinema->name) + "_" + tidy_for_filename (i->screen->name) + ".kdm.xml";
					i->kdm.as_xml (out);
				}
			} else {
				while (!screen_kdms.empty ()) {

					/* Get all the screens from a single cinema */

					shared_ptr<Cinema> cinema;
					list<ScreenKDM> cinema_screen_kdms;

					list<ScreenKDM>::iterator i = screen_kdms.begin ();
					cinema = i->screen->cinema;
					cinema_screen_kdms.push_back (*i);
					list<ScreenKDM>::iterator j = i;
					++i;
					screen_kdms.remove (*j);

					while (i != screen_kdms.end ()) {
						if (i->screen->cinema == cinema) {
							cinema_screen_kdms.push_back (*i);
							list<ScreenKDM>::iterator j = i;
							++i;
							screen_kdms.remove (*j);
						} else {
							++i;
						}
					}

					/* Make a ZIP file of this cinema's KDMs */
					
					boost::filesystem::path zip_file = boost::filesystem::temp_directory_path ();
					zip_file /= boost::filesystem::unique_path().string() + ".zip";
					struct zip* zip = zip_open (zip_file.string().c_str(), ZIP_CREATE | ZIP_EXCL, 0);
					if (!zip) {
						throw FileError ("could not create ZIP file", zip_file);
					}

					list<shared_ptr<string> > kdm_strings;

					for (list<ScreenKDM>::const_iterator i = cinema_screen_kdms.begin(); i != cinema_screen_kdms.end(); ++i) {
						shared_ptr<string> kdm (new string (i->kdm.as_xml ()));
						kdm_strings.push_back (kdm);
						
						struct zip_source* source = zip_source_buffer (zip, kdm->c_str(), kdm->length(), 0);
						if (!source) {
							throw StringError ("could not create ZIP source");
						}
						
						string const name = tidy_for_filename (i->screen->cinema->name) + "_" +
							tidy_for_filename (i->screen->name) + ".kdm.xml";
						
						if (zip_add (zip, name.c_str(), source) == -1) {
							throw StringError ("failed to add KDM to ZIP archive");
						}
					}

					if (zip_close (zip) == -1) {
						throw StringError ("failed to close ZIP archive");
					}

					/* Send email */

					quickmail_initialize ();
					quickmail mail = quickmail_create (Config::instance()->kdm_from().c_str(), "KDM delivery");
					quickmail_add_to (mail, cinema->email.c_str ());

					string body = Config::instance()->kdm_email().c_str();
					boost::algorithm::replace_all (body, "$DCP_NAME", film->dcp_name ());
					
					quickmail_set_body (mail, body.c_str());
					quickmail_add_attachment_file (mail, zip_file.string().c_str());
					char const* error = quickmail_send (mail, Config::instance()->mail_server().c_str(), 25, "", "");
					if (error) {
						quickmail_destroy (mail);
						throw StringError (String::compose ("Failed to send KDM email (%1)", error));
					}
					quickmail_destroy (mail);

					film->log()->log (String::compose ("Send KDM email to %1", cinema->email));
				}
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

		ev.Skip ();
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
