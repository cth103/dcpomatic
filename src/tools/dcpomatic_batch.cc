/*
    Copyright (C) 2013-2018 Carl Hetherington <cth@carlh.net>

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

#include "wx/wx_util.h"
#include "wx/about_dialog.h"
#include "wx/wx_signal_manager.h"
#include "wx/job_manager_view.h"
#include "wx/full_config_dialog.h"
#include "wx/servers_list_dialog.h"
#include "lib/version.h"
#include "lib/compose.hpp"
#include "lib/config.h"
#include "lib/util.h"
#include "lib/film.h"
#include "lib/job_manager.h"
#include "lib/dcpomatic_socket.h"
#include <wx/aboutdlg.h>
#include <wx/stdpaths.h>
#include <wx/cmdline.h>
#include <wx/splash.h>
#include <wx/preferences.h>
#include <wx/wx.h>
#include <boost/foreach.hpp>
#include <iostream>

using std::exception;
using std::string;
using std::cout;
using std::list;
using boost::shared_ptr;
using boost::thread;
using boost::scoped_array;

static list<boost::filesystem::path> films_to_load;

enum {
	ID_file_add_film = 1,
	ID_tools_encoding_servers,
	ID_help_about
};

void
setup_menu (wxMenuBar* m)
{
	wxMenu* file = new wxMenu;
	file->Append (ID_file_add_film, _("&Add Film...\tCtrl-A"));
#ifdef DCPOMATIC_OSX
	file->Append (wxID_EXIT, _("&Exit"));
#else
	file->Append (wxID_EXIT, _("&Quit"));
#endif

#ifdef DCPOMATIC_OSX
	file->Append (wxID_PREFERENCES, _("&Preferences...\tCtrl-P"));
#else
	wxMenu* edit = new wxMenu;
	edit->Append (wxID_PREFERENCES, _("&Preferences...\tCtrl-P"));
#endif

	wxMenu* tools = new wxMenu;
	tools->Append (ID_tools_encoding_servers, _("Encoding servers..."));

	wxMenu* help = new wxMenu;
	help->Append (ID_help_about, _("About"));

	m->Append (file, _("&File"));
#ifndef DCPOMATIC_OSX
	m->Append (edit, _("&Edit"));
#endif
	m->Append (tools, _("&Tools"));
	m->Append (help, _("&Help"));
}

class DOMFrame : public wxFrame
{
public:
	explicit DOMFrame (wxString const & title)
		: wxFrame (NULL, -1, title)
		, _sizer (new wxBoxSizer (wxVERTICAL))
		, _config_dialog (0)
		, _servers_list_dialog (0)
	{
		wxMenuBar* bar = new wxMenuBar;
		setup_menu (bar);
		SetMenuBar (bar);

		Config::instance()->Changed.connect (boost::bind (&DOMFrame::config_changed, this, _1));

		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_add_film, this),    ID_file_add_film);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_quit, this),        wxID_EXIT);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::edit_preferences, this), wxID_PREFERENCES);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_encoding_servers, this), ID_tools_encoding_servers);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::help_about, this),       ID_help_about);

		wxPanel* panel = new wxPanel (this);
		wxSizer* s = new wxBoxSizer (wxHORIZONTAL);
		s->Add (panel, 1, wxEXPAND);
		SetSizer (s);

		JobManagerView* job_manager_view = new JobManagerView (panel, true);
		_sizer->Add (job_manager_view, 1, wxALL | wxEXPAND, 6);

		wxSizer* buttons = new wxBoxSizer (wxHORIZONTAL);
		wxButton* add = new wxButton (panel, wxID_ANY, _("Add Film..."));
		add->Bind (wxEVT_BUTTON, boost::bind (&DOMFrame::add_film, this));
		buttons->Add (add, 1, wxALL, 6);

		_sizer->Add (buttons, 0, wxALL, 6);

		panel->SetSizer (_sizer);

		Bind (wxEVT_CLOSE_WINDOW, boost::bind (&DOMFrame::close, this, _1));
		Bind (wxEVT_SIZE, boost::bind (&DOMFrame::sized, this, _1));
	}

	void start_job (boost::filesystem::path path)
	{
		try {
			shared_ptr<Film> film (new Film (path));
			film->read_metadata ();
			film->make_dcp ();
		} catch (std::exception& e) {
			wxString p = std_to_wx (path.string ());
			wxCharBuffer b = p.ToUTF8 ();
			error_dialog (this, wxString::Format (_("Could not open film at %s"), p.data()), std_to_wx(e.what()));
		}
	}

private:
	void sized (wxSizeEvent& ev)
	{
		_sizer->Layout ();
		ev.Skip ();
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

	void file_add_film ()
	{
		add_film ();
	}

	void file_quit ()
	{
		if (should_close ()) {
			Close (true);
		}
	}

	void edit_preferences ()
	{
		if (!_config_dialog) {
			_config_dialog = create_full_config_dialog ();
		}
		_config_dialog->Show (this);
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

	void add_film ()
	{
		wxDirDialog* c = new wxDirDialog (this, _("Select film to open"), wxStandardPaths::Get().GetDocumentsDir(), wxDEFAULT_DIALOG_STYLE | wxDD_DIR_MUST_EXIST);
		if (_last_parent) {
			c->SetPath (std_to_wx (_last_parent.get().string ()));
		}

		int r;
		while (true) {
			r = c->ShowModal ();
			if (r == wxID_OK && c->GetPath() == wxStandardPaths::Get().GetDocumentsDir()) {
				error_dialog (this, _("You did not select a folder.  Make sure that you select a folder before clicking Open."));
			} else {
				break;
			}
		}

		if (r == wxID_OK) {
			start_job (wx_to_std (c->GetPath ()));
		}

		_last_parent = boost::filesystem::path (wx_to_std (c->GetPath ())).parent_path ();

		c->Destroy ();
	}

	void config_changed (Config::Property what)
	{
		/* Instantly save any config changes when using the DCP-o-matic GUI */
		if (what == Config::CINEMAS) {
			try {
				Config::instance()->write_cinemas();
			} catch (exception& e) {
				error_dialog (
					this,
					wxString::Format (
						_("Could not write to cinemas file at %s.  Your changes have not been saved."),
						std_to_wx (Config::instance()->cinemas_file().string()).data()
						)
					);
			}
		} else {
			try {
				Config::instance()->write_config();
			} catch (exception& e) {
				error_dialog (
					this,
					wxString::Format (
						_("Could not write to config file at %s.  Your changes have not been saved."),
						std_to_wx (Config::instance()->cinemas_file().string()).data()
						)
					);
			}
		}
	}

	boost::optional<boost::filesystem::path> _last_parent;
	wxSizer* _sizer;
	wxPreferencesEditor* _config_dialog;
	ServersListDialog* _servers_list_dialog;
};

static const wxCmdLineEntryDesc command_line_description[] = {
	{ wxCMD_LINE_PARAM, 0, 0, "film to load", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_MULTIPLE | wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_NONE, "", "", "", wxCmdLineParamType (0), 0 }
};

class JobServer : public Server
{
public:
	explicit JobServer (DOMFrame* frame)
		: Server (BATCH_JOB_PORT)
		, _frame (frame)
	{}

	void handle (shared_ptr<Socket> socket)
	{
		try {
			int const length = socket->read_uint32 ();
			scoped_array<char> buffer (new char[length]);
			socket->read (reinterpret_cast<uint8_t*> (buffer.get()), length);
			string s (buffer.get());
			_frame->start_job (s);
			socket->write (reinterpret_cast<uint8_t const *> ("OK"), 3);
		} catch (...) {

		}
	}

private:
	DOMFrame* _frame;
};

class App : public wxApp
{
	bool OnInit ()
	{
		SetAppName (_("DCP-o-matic Batch Converter"));
		is_batch_converter = true;

		Config::FailedToLoad.connect (boost::bind (&App::config_failed_to_load, this));
		Config::Warning.connect (boost::bind (&App::config_warning, this, _1));

		wxSplashScreen* splash = maybe_show_splash ();

		if (!wxApp::OnInit()) {
			return false;
		}

#ifdef DCPOMATIC_LINUX
		unsetenv ("UBUNTU_MENUPROXY");
#endif

		dcpomatic_setup_path_encoding ();

		/* Enable i18n; this will create a Config object
		   to look for a force-configured language.  This Config
		   object will be wrong, however, because dcpomatic_setup
		   hasn't yet been called and there aren't any filters etc.
		   set up yet.
		*/
		dcpomatic_setup_i18n ();

		/* Set things up, including filters etc.
		   which will now be internationalised correctly.
		*/
		dcpomatic_setup ();

		/* Force the configuration to be re-loaded correctly next
		   time it is needed.
		*/
		Config::drop ();

		_frame = new DOMFrame (_("DCP-o-matic Batch Converter"));
		SetTopWindow (_frame);
		_frame->Maximize ();
		if (splash) {
			splash->Destroy ();
		}
		_frame->Show ();

		JobServer* server = new JobServer (_frame);
		new thread (boost::bind (&JobServer::run, server));

		signal_manager = new wxSignalManager (this);
		this->Bind (wxEVT_IDLE, boost::bind (&App::idle, this));

		shared_ptr<Film> film;
		BOOST_FOREACH (boost::filesystem::path i, films_to_load) {
			if (boost::filesystem::is_directory (i)) {
				try {
					film.reset (new Film (i));
					film->read_metadata ();
					film->make_dcp ();
				} catch (exception& e) {
					error_dialog (
						0,
						std_to_wx (String::compose (wx_to_std (_("Could not load film %1")), i.string())),
						std_to_wx(e.what())
						);
				}
			}
		}

		return true;
	}

	void idle ()
	{
		signal_manager->ui_idle ();
	}

	void OnInitCmdLine (wxCmdLineParser& parser)
	{
		parser.SetDesc (command_line_description);
		parser.SetSwitchChars (wxT ("-"));
	}

	bool OnCmdLineParsed (wxCmdLineParser& parser)
	{
		for (size_t i = 0; i < parser.GetParamCount(); ++i) {
			films_to_load.push_back (wx_to_std (parser.GetParam(i)));
		}

		return true;
	}

	void config_failed_to_load ()
	{
		message_dialog (_frame, _("The existing configuration failed to load.  Default values will be used instead.  These may take a short time to create."));
	}

	void config_warning (string m)
	{
		message_dialog (_frame, std_to_wx (m));
	}

	DOMFrame* _frame;
};

IMPLEMENT_APP (App)
