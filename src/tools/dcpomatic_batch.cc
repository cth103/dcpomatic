/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#include "wx/about_dialog.h"
#include "wx/dcpomatic_button.h"
#include "wx/full_config_dialog.h"
#include "wx/job_manager_view.h"
#include "wx/servers_list_dialog.h"
#include "wx/wx_ptr.h"
#include "wx/wx_signal_manager.h"
#include "wx/wx_util.h"
#include "lib/compose.hpp"
#include "lib/config.h"
#include "lib/dcpomatic_socket.h"
#include "lib/film.h"
#ifdef DCPOMATIC_GROK
#include "lib/grok/context.h"
#endif
#include "lib/job.h"
#include "lib/job_manager.h"
#include "lib/make_dcp.h"
#include "lib/transcode_job.h"
#include "lib/util.h"
#include "lib/version.h"
#include <dcp/filesystem.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/aboutdlg.h>
#include <wx/cmdline.h>
#include <wx/dnd.h>
#include <wx/preferences.h>
#include <wx/splash.h>
#include <wx/stdpaths.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <iostream>
#include <set>


using std::cout;
using std::dynamic_pointer_cast;
using std::exception;
using std::list;
using std::make_shared;
using std::set;
using std::shared_ptr;
using std::string;
using boost::scoped_array;
using boost::thread;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


static list<boost::filesystem::path> films_to_load;


enum {
	ID_file_add_film = 1,
	ID_tools_encoding_servers,
	ID_help_about
};


void
setup_menu (wxMenuBar* m)
{
	auto file = new wxMenu;
	file->Append (ID_file_add_film, _("&Add Film...\tCtrl-A"));
#ifdef DCPOMATIC_OSX
	file->Append (wxID_EXIT, _("&Exit"));
#else
	file->Append (wxID_EXIT, _("&Quit"));
#endif

#ifdef DCPOMATIC_OSX
	file->Append (wxID_PREFERENCES, _("&Preferences...\tCtrl-P"));
#else
	auto edit = new wxMenu;
	edit->Append (wxID_PREFERENCES, _("&Preferences...\tCtrl-P"));
#endif

	auto tools = new wxMenu;
	tools->Append (ID_tools_encoding_servers, _("Encoding servers..."));

	auto help = new wxMenu;
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
	enum class Tool {
		ADD,
		PAUSE
	};

	class DCPDropTarget : public wxFileDropTarget
	{
	public:
		DCPDropTarget(DOMFrame* owner)
			: _frame(owner)
		{}

		bool OnDropFiles(wxCoord, wxCoord, wxArrayString const& filenames) override
		{
			if (filenames.GetCount() == 1) {
				/* Try to load a directory */
				auto path = boost::filesystem::path(wx_to_std(filenames[0]));
				if (dcp::filesystem::is_directory(path)) {
					_frame->start_job(wx_to_std(filenames[0]));
					return true;
				}
			}

			return false;
		}

	private:
		DOMFrame* _frame;
	};

	explicit DOMFrame (wxString const & title)
		: wxFrame (nullptr, -1, title)
		, _sizer (new wxBoxSizer(wxVERTICAL))
	{
		auto bar = new wxMenuBar;
		setup_menu (bar);
		SetMenuBar (bar);

		Config::instance()->Changed.connect (boost::bind (&DOMFrame::config_changed, this, _1));

		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_add_film, this),    ID_file_add_film);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_quit, this),        wxID_EXIT);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::edit_preferences, this), wxID_PREFERENCES);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_encoding_servers, this), ID_tools_encoding_servers);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::help_about, this),       ID_help_about);

		auto panel = new wxPanel (this);
		auto s = new wxBoxSizer (wxHORIZONTAL);
		s->Add (panel, 1, wxEXPAND);
		SetSizer (s);

		wxBitmap add(icon_path("add"), wxBITMAP_TYPE_PNG);
		wxBitmap pause(icon_path("pause"), wxBITMAP_TYPE_PNG);

		auto toolbar = new wxToolBar(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL);
		toolbar->SetMargins(4, 4);
		toolbar->SetToolBitmapSize(wxSize(32, 32));
		toolbar->AddTool(static_cast<int>(Tool::ADD), _("Add film"), add, _("Add film for conversion"));
		toolbar->AddCheckTool(static_cast<int>(Tool::PAUSE), _("Pause/resume"), pause, wxNullBitmap, _("Pause or resume conversion"));
		toolbar->Realize();
		_sizer->Add(toolbar, 0, wxALL, 6);

		toolbar->Bind(wxEVT_TOOL, bind(&DOMFrame::tool_clicked, this, _1));

		auto job_manager_view = new JobManagerView (panel, true);
		_sizer->Add (job_manager_view, 1, wxALL | wxEXPAND, 6);

		panel->SetSizer (_sizer);

		Bind (wxEVT_CLOSE_WINDOW, boost::bind(&DOMFrame::close, this, _1));
		Bind (wxEVT_SIZE, boost::bind(&DOMFrame::sized, this, _1));

		SetDropTarget(new DCPDropTarget(this));
	}

	void tool_clicked(wxCommandEvent& ev)
	{
		switch (static_cast<Tool>(ev.GetId())) {
		case Tool::ADD:
			add_film();
			break;
		case Tool::PAUSE:
		{
			auto jm = JobManager::instance();
			if (jm->paused()) {
				jm->resume();
			} else {
				jm->pause();
			}
			break;
		}
		}
	}

	void start_job (boost::filesystem::path path)
	{
		try {
			auto film = make_shared<Film>(path);
			film->read_metadata ();

			double total_required;
			double available;
			bool can_hard_link;

			film->should_be_enough_disk_space (total_required, available, can_hard_link);

			set<shared_ptr<const Film>> films;

			for (auto i: JobManager::instance()->get()) {
				films.insert (i->film());
			}

			for (auto i: films) {
				double progress = 0;
				for (auto j: JobManager::instance()->get()) {
					if (i == j->film() && dynamic_pointer_cast<TranscodeJob>(j)) {
						progress = j->progress().get_value_or(0);
					}
				}

				double required;
				i->should_be_enough_disk_space (required, available, can_hard_link);
				total_required += (1 - progress) * required;
			}

			if ((total_required - available) > 1) {
				if (!confirm_dialog (
					    this,
					    wxString::Format(
						    _("The DCPs for this film and the films already in the queue will take up about %.1f GB.  The "
						      "disks that you are using only have %.1f GB available.  Do you want to add this film to the queue anyway?"),
						    total_required, available))) {
					return;
				}
			}

			make_dcp (film, TranscodeJob::ChangedBehaviour::STOP);
		} catch (std::exception& e) {
			auto p = std_to_wx (path.string ());
			auto b = p.ToUTF8 ();
			error_dialog (this, wxString::Format(_("Could not open film at %s"), p.data()), std_to_wx(e.what()));
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
		if (!JobManager::instance()->work_to_do()) {
			return true;
		}

		auto d = make_wx<wxMessageDialog>(
			nullptr,
			_("There are unfinished jobs; are you sure you want to quit?"),
			_("Unfinished jobs"),
			wxYES_NO | wxYES_DEFAULT | wxICON_QUESTION
			);

		return d->ShowModal() == wxID_YES;
	}

	void close (wxCloseEvent& ev)
	{
		if (!should_close()) {
			ev.Veto ();
			return;
		}

		ev.Skip ();
		JobManager::drop ();
	}

	void file_add_film ()
	{
		add_film ();
	}

	void file_quit ()
	{
		if (should_close()) {
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
		auto d = make_wx<AboutDialog>(this);
		d->ShowModal ();
	}

	void add_film ()
	{
		auto dialog = make_wx<wxDirDialog>(this, _("Select film to open"), wxStandardPaths::Get().GetDocumentsDir(), wxDEFAULT_DIALOG_STYLE | wxDD_DIR_MUST_EXIST);
		if (_last_parent) {
			dialog->SetPath(std_to_wx(_last_parent.get().string()));
		}

		int r;
		while (true) {
			r = dialog->ShowModal();
			if (r == wxID_OK && dialog->GetPath() == wxStandardPaths::Get().GetDocumentsDir()) {
				error_dialog (this, _("You did not select a folder.  Make sure that you select a folder before clicking Open."));
			} else {
				break;
			}
		}

		if (r == wxID_OK) {
			start_job(wx_to_std(dialog->GetPath()));
		}

		_last_parent = boost::filesystem::path(wx_to_std(dialog->GetPath())).parent_path();
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
					wxString::Format(
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
					wxString::Format(
						_("Could not write to config file at %s.  Your changes have not been saved."),
						std_to_wx (Config::instance()->cinemas_file().string()).data()
						)
					);
			}
		}
	}

	boost::optional<boost::filesystem::path> _last_parent;
	wxSizer* _sizer;
	wxPreferencesEditor* _config_dialog = nullptr;
	ServersListDialog* _servers_list_dialog = nullptr;
};


static const wxCmdLineEntryDesc command_line_description[] = {
	{ wxCMD_LINE_PARAM, 0, 0, "film to load", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_MULTIPLE | wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_NONE, "", "", "", wxCmdLineParamType (0), 0 }
};


class JobServer : public Server, public Signaller
{
public:
	JobServer()
		: Server (BATCH_JOB_PORT)
	{}

	void handle (shared_ptr<Socket> socket) override
	{
		try {
			auto const length = socket->read_uint32();
			if (length < 65536) {
				scoped_array<char> buffer(new char[length]);
				socket->read(reinterpret_cast<uint8_t*>(buffer.get()), length);
				string s(buffer.get());
				emit(boost::bind(boost::ref(StartJob), s));
				socket->write (reinterpret_cast<uint8_t const *>("OK"), 3);
			}
		} catch (...) {

		}
	}

	boost::signals2::signal<void(std::string)> StartJob;
};


class App : public wxApp
{
	bool OnInit () override
	{
		wxInitAllImageHandlers ();

		SetAppName (_("DCP-o-matic Batch Converter"));
		is_batch_converter = true;

		Config::FailedToLoad.connect(boost::bind(&App::config_failed_to_load, this, _1));
		Config::Warning.connect (boost::bind(&App::config_warning, this, _1));

		auto splash = maybe_show_splash ();

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

		try {
			auto server = new JobServer();
			server->StartJob.connect(bind(&DOMFrame::start_job, _frame, _1));
			new thread (boost::bind (&JobServer::run, server));
		} catch (boost::system::system_error& e) {
			error_dialog(_frame, _("Could not listen for new batch jobs.  Perhaps another instance of the DCP-o-matic Batch Converter is running."));
		}

		signal_manager = new wxSignalManager (this);
		this->Bind (wxEVT_IDLE, boost::bind (&App::idle, this));

		shared_ptr<Film> film;
		for (auto i: films_to_load) {
			if (dcp::filesystem::is_directory(i)) {
				try {
					film = make_shared<Film>(i);
					film->read_metadata ();
					make_dcp (film, TranscodeJob::ChangedBehaviour::EXAMINE_THEN_STOP);
				} catch (exception& e) {
					error_dialog (
						0,
						std_to_wx(String::compose(wx_to_std(_("Could not load film %1")), i.string())),
						std_to_wx(e.what())
						);
				}
			}
		}

#ifdef DCPOMATIC_GROK
		grk_plugin::setMessengerLogger(new grk_plugin::GrokLogger("[GROK] "));
#endif

		return true;
	}

	void idle ()
	{
		signal_manager->ui_idle ();
	}

	void OnInitCmdLine (wxCmdLineParser& parser) override
	{
		parser.SetDesc (command_line_description);
		parser.SetSwitchChars (wxT ("-"));
	}

	bool OnCmdLineParsed (wxCmdLineParser& parser) override
	{
		for (size_t i = 0; i < parser.GetParamCount(); ++i) {
			films_to_load.push_back (wx_to_std(parser.GetParam(i)));
		}

		return true;
	}

	void config_failed_to_load(Config::LoadFailure what)
	{
		report_config_load_failure(_frame, what);
	}

	void config_warning (string m)
	{
		message_dialog (_frame, std_to_wx(m));
	}

	DOMFrame* _frame;
};


IMPLEMENT_APP (App)
