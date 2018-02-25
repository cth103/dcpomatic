/*
    Copyright (C) 2017-2018 Carl Hetherington <cth@carlh.net>

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

#include "lib/cross.h"
#include "lib/config.h"
#include "lib/util.h"
#include "lib/update_checker.h"
#include "lib/compose.hpp"
#include "lib/dcp_content.h"
#include "lib/job_manager.h"
#include "lib/job.h"
#include "lib/video_content.h"
#include "lib/subtitle_content.h"
#include "lib/ratio.h"
#include "wx/wx_signal_manager.h"
#include "wx/wx_util.h"
#include "wx/about_dialog.h"
#include "wx/report_problem_dialog.h"
#include "wx/film_viewer.h"
#include "wx/player_information.h"
#include "wx/update_dialog.h"
#include "wx/player_config_dialog.h"
#include <wx/wx.h>
#include <wx/stdpaths.h>
#include <wx/splash.h>
#include <wx/cmdline.h>
#include <wx/preferences.h>
#include <wx/progdlg.h>
#ifdef __WXOSX__
#include <ApplicationServices/ApplicationServices.h>
#endif
#include <boost/bind.hpp>
#include <iostream>

#ifdef check
#undef check
#endif

using std::string;
using std::cout;
using std::exception;
using std::vector;
using boost::shared_ptr;
using boost::optional;

enum {
	ID_file_open = 1,
	ID_file_add_ov,
	ID_file_add_kdm,
	ID_file_history,
	/* Allow spare IDs after _history for the recent files list */
	ID_file_close = 100,
	ID_view_scale_appropriate,
	ID_view_scale_full,
	ID_view_scale_half,
	ID_view_scale_quarter,
	ID_help_report_a_problem,
	ID_tools_check_for_updates,
};

class DOMFrame : public wxFrame
{
public:
	DOMFrame ()
		: wxFrame (0, -1, _("DCP-o-matic Player"))
		, _update_news_requested (false)
		, _info (0)
		, _config_dialog (0)
		, _file_menu (0)
		, _history_items (0)
		, _history_position (0)
		, _history_separator (0)
		, _viewer (0)
	{

#if defined(DCPOMATIC_WINDOWS)
		maybe_open_console ();
		cout << "DCP-o-matic Player is starting." << "\n";
#endif

		wxMenuBar* bar = new wxMenuBar;
		setup_menu (bar);
		set_menu_sensitivity ();
		SetMenuBar (bar);

#ifdef DCPOMATIC_WINDOWS
		SetIcon (wxIcon (std_to_wx ("id")));
#endif

		_config_changed_connection = Config::instance()->Changed.connect (boost::bind (&DOMFrame::config_changed, this));
		config_changed ();

		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_open, this), ID_file_open);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_add_ov, this), ID_file_add_ov);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_add_kdm, this), ID_file_add_kdm);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_history, this, _1), ID_file_history, ID_file_history + HISTORY_SIZE);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_close, this), ID_file_close);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_exit, this), wxID_EXIT);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::edit_preferences, this), wxID_PREFERENCES);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::set_decode_reduction, this, optional<int>()), ID_view_scale_appropriate);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::set_decode_reduction, this, optional<int>(0)), ID_view_scale_full);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::set_decode_reduction, this, optional<int>(1)), ID_view_scale_half);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::set_decode_reduction, this, optional<int>(2)), ID_view_scale_quarter);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::help_about, this), wxID_ABOUT);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::help_report_a_problem, this), ID_help_report_a_problem);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_check_for_updates, this), ID_tools_check_for_updates);

		/* Use a panel as the only child of the Frame so that we avoid
		   the dark-grey background on Windows.
		*/
		wxPanel* overall_panel = new wxPanel (this, wxID_ANY);

		_viewer = new FilmViewer (overall_panel, false, false);
		_viewer->set_dcp_decode_reduction (Config::instance()->decode_reduction ());
		_info = new PlayerInformation (overall_panel, _viewer);
		wxSizer* main_sizer = new wxBoxSizer (wxVERTICAL);
		main_sizer->Add (_viewer, 1, wxEXPAND | wxALL, 6);
		main_sizer->Add (_info, 0, wxEXPAND | wxALL, 6);
		overall_panel->SetSizer (main_sizer);

#ifdef __WXOSX__
		wxAcceleratorEntry* accel = new wxAcceleratorEntry[1];
		accel[0].Set(wxACCEL_CTRL, static_cast<int>('W'), ID_file_close);
		wxAcceleratorTable accel_table (1, accel);
		SetAcceleratorTable (accel_table);
		delete[] accel;
#endif

		UpdateChecker::instance()->StateChanged.connect (boost::bind (&DOMFrame::update_checker_state_changed, this));
	}

	void set_decode_reduction (optional<int> reduction)
	{
		_viewer->set_dcp_decode_reduction (reduction);
		_info->triggered_update ();
		Config::instance()->set_decode_reduction (reduction);
	}

	void load_dcp (boost::filesystem::path dir)
	{
		_film.reset (new Film (optional<boost::filesystem::path>()));
		shared_ptr<DCPContent> dcp;
		try {
			dcp.reset (new DCPContent (_film, dir));
		} catch (boost::filesystem::filesystem_error& e) {
			error_dialog (this, _("Could not load DCP"), std_to_wx (e.what()));
			return;
		}

		_film->examine_and_add_content (dcp, true);

		JobManager* jm = JobManager::instance ();

		wxProgressDialog* progress = new wxProgressDialog (_("DCP-o-matic Player"), _("Loading DCP"));

		while (jm->work_to_do() || signal_manager->ui_idle()) {
			dcpomatic_sleep (1);
			progress->Pulse ();
		}

		progress->Destroy ();

		DCPOMATIC_ASSERT (!jm->get().empty());

		shared_ptr<Job> last = jm->get().back();
		if (last->finished_in_error()) {
			error_dialog(this, std_to_wx(last->error_summary()) + ".\n", std_to_wx(last->error_details()));
			return;
		}

		if (dcp->subtitle) {
			dcp->subtitle->set_use (true);
		}

		Ratio const * r = Ratio::nearest_from_ratio(dcp->video->size().ratio());
		if (r) {
			_film->set_container(r);
		}

		_viewer->set_film (_film);
		_viewer->set_position (DCPTime ());
		_info->triggered_update ();

		Config::instance()->add_to_player_history (dir);

		set_menu_sensitivity ();
	}

private:

	void setup_menu (wxMenuBar* m)
	{
		_file_menu = new wxMenu;
		_file_menu->Append (ID_file_open, _("&Open...\tCtrl-O"));
		_file_add_ov = _file_menu->Append (ID_file_add_ov, _("&Add OV..."));
		_file_add_kdm = _file_menu->Append (ID_file_add_kdm, _("&Add KDM..."));

		_history_position = _file_menu->GetMenuItems().GetCount();

		_file_menu->AppendSeparator ();
		_file_menu->Append (ID_file_close, _("&Close"));
		_file_menu->AppendSeparator ();

#ifdef __WXOSX__
		_file_menu->Append (wxID_EXIT, _("&Exit"));
#else
		_file_menu->Append (wxID_EXIT, _("&Quit"));
#endif

#ifdef __WXOSX__
		_file_menu->Append (wxID_PREFERENCES, _("&Preferences...\tCtrl-P"));
#else
		wxMenu* edit = new wxMenu;
		edit->Append (wxID_PREFERENCES, _("&Preferences...\tCtrl-P"));
#endif

		wxMenu* view = new wxMenu;
		optional<int> c = Config::instance()->decode_reduction();
		view->AppendRadioItem(ID_view_scale_appropriate, _("Set decode resolution to match display"))->Check(!static_cast<bool>(c));
		view->AppendRadioItem(ID_view_scale_full, _("Decode at full resolution"))->Check(c && c.get() == 0);
		view->AppendRadioItem(ID_view_scale_half, _("Decode at half resolution"))->Check(c && c.get() == 1);
		view->AppendRadioItem(ID_view_scale_quarter, _("Decode at quarter resolution"))->Check(c && c.get() == 2);

		wxMenu* tools = new wxMenu;
		tools->Append (ID_tools_check_for_updates, _("Check for updates"));

		wxMenu* help = new wxMenu;
#ifdef __WXOSX__
		help->Append (wxID_ABOUT, _("About DCP-o-matic"));
#else
		help->Append (wxID_ABOUT, _("About"));
#endif
		help->Append (ID_help_report_a_problem, _("Report a problem..."));

		m->Append (_file_menu, _("&File"));
#ifndef __WXOSX__
		m->Append (edit, _("&Edit"));
#endif
		m->Append (view, _("&View"));
		m->Append (tools, _("&Tools"));
		m->Append (help, _("&Help"));
	}

	void file_open ()
	{
		wxString d = wxStandardPaths::Get().GetDocumentsDir();
		if (Config::instance()->last_player_load_directory()) {
			d = std_to_wx (Config::instance()->last_player_load_directory()->string());
		}

		wxDirDialog* c = new wxDirDialog (this, _("Select DCP to open"), d, wxDEFAULT_DIALOG_STYLE | wxDD_DIR_MUST_EXIST);

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
			boost::filesystem::path const dcp (wx_to_std (c->GetPath ()));
			load_dcp (dcp);
			Config::instance()->set_last_player_load_directory (dcp.parent_path());
		}

		c->Destroy ();
	}

	void file_add_ov ()
	{
		wxDirDialog* c = new wxDirDialog (
			this,
			_("Select DCP to open as OV"),
			wxStandardPaths::Get().GetDocumentsDir(),
			wxDEFAULT_DIALOG_STYLE | wxDD_DIR_MUST_EXIST
			);

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
			DCPOMATIC_ASSERT (_film);
			shared_ptr<DCPContent> dcp = boost::dynamic_pointer_cast<DCPContent>(_film->content().front());
			DCPOMATIC_ASSERT (dcp);
			dcp->add_ov (wx_to_std(c->GetPath()));
			dcp->examine (shared_ptr<Job>());
			/* Maybe we just gained some subtitles */
			if (dcp->subtitle) {
				dcp->subtitle->set_use (true);
			}
		}

		c->Destroy ();
		_info->triggered_update ();
	}

	void file_add_kdm ()
	{
		wxFileDialog* d = new wxFileDialog (this, _("Select KDM"));

		if (d->ShowModal() == wxID_OK) {
			DCPOMATIC_ASSERT (_film);
			shared_ptr<DCPContent> dcp = boost::dynamic_pointer_cast<DCPContent>(_film->content().front());
			DCPOMATIC_ASSERT (dcp);
			try {
				dcp->add_kdm (dcp::EncryptedKDM (dcp::file_to_string (wx_to_std (d->GetPath ()), MAX_KDM_SIZE)));
				dcp->examine (shared_ptr<Job>());
			} catch (exception& e) {
				error_dialog (this, wxString::Format (_("Could not load KDM.")), std_to_wx(e.what()));
				d->Destroy ();
				return;
			}
		}

		d->Destroy ();
		_info->triggered_update ();
	}

	void file_history (wxCommandEvent& event)
	{
		vector<boost::filesystem::path> history = Config::instance()->player_history ();
		int n = event.GetId() - ID_file_history;
		if (n >= 0 && n < static_cast<int> (history.size ())) {
			load_dcp (history[n]);
		}
	}

	void file_close ()
	{
		_viewer->set_film (shared_ptr<Film>());
		_film.reset ();
		_info->triggered_update ();
		set_menu_sensitivity ();
	}

	void file_exit ()
	{
		Close ();
	}

	void edit_preferences ()
	{
		if (!_config_dialog) {
			_config_dialog = create_player_config_dialog ();
		}
		_config_dialog->Show (this);
	}

	void tools_check_for_updates ()
	{
		UpdateChecker::instance()->run ();
		_update_news_requested = true;
	}

	void help_about ()
	{
		AboutDialog* d = new AboutDialog (this);
		d->ShowModal ();
		d->Destroy ();
	}

	void help_report_a_problem ()
	{
		ReportProblemDialog* d = new ReportProblemDialog (this);
		if (d->ShowModal () == wxID_OK) {
			d->report ();
		}
		d->Destroy ();
	}

	void update_checker_state_changed ()
	{
		UpdateChecker* uc = UpdateChecker::instance ();

		bool const announce =
			_update_news_requested ||
			(uc->stable() && Config::instance()->check_for_updates()) ||
			(uc->test() && Config::instance()->check_for_updates() && Config::instance()->check_for_test_updates());

		_update_news_requested = false;

		if (!announce) {
			return;
		}

		if (uc->state() == UpdateChecker::YES) {
			UpdateDialog* dialog = new UpdateDialog (this, uc->stable (), uc->test ());
			dialog->ShowModal ();
			dialog->Destroy ();
		} else if (uc->state() == UpdateChecker::FAILED) {
			error_dialog (this, _("The DCP-o-matic download server could not be contacted."));
		} else {
			error_dialog (this, _("There are no new versions of DCP-o-matic available."));
		}

		_update_news_requested = false;
	}

	void config_changed ()
	{
		/* Instantly save any config changes when using the player GUI */
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

		for (int i = 0; i < _history_items; ++i) {
			delete _file_menu->Remove (ID_file_history + i);
		}

		if (_history_separator) {
			_file_menu->Remove (_history_separator);
		}
		delete _history_separator;
		_history_separator = 0;

		int pos = _history_position;

		vector<boost::filesystem::path> history = Config::instance()->player_history ();

		if (!history.empty ()) {
			_history_separator = _file_menu->InsertSeparator (pos++);
		}

		for (size_t i = 0; i < history.size(); ++i) {
			string s;
			if (i < 9) {
				s = String::compose ("&%1 %2", i + 1, history[i].string());
			} else {
				s = history[i].string();
			}
			_file_menu->Insert (pos++, ID_file_history + i, std_to_wx (s));
		}

		_history_items = history.size ();
	}

	void set_menu_sensitivity ()
	{
		_file_add_ov->Enable (static_cast<bool>(_film));
		_file_add_kdm->Enable (static_cast<bool>(_film));
	}

	bool _update_news_requested;
	PlayerInformation* _info;
	wxPreferencesEditor* _config_dialog;
	wxMenu* _file_menu;
	int _history_items;
	int _history_position;
	wxMenuItem* _history_separator;
	FilmViewer* _viewer;
	boost::shared_ptr<Film> _film;
	boost::signals2::scoped_connection _config_changed_connection;
	wxMenuItem* _file_add_ov;
	wxMenuItem* _file_add_kdm;
};

static const wxCmdLineEntryDesc command_line_description[] = {
	{ wxCMD_LINE_PARAM, 0, 0, "DCP to load or create", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_NONE, "", "", "", wxCmdLineParamType (0), 0 }
};

/** @class App
 *  @brief The magic App class for wxWidgets.
 */
class App : public wxApp
{
public:
	App ()
		: wxApp ()
		, _frame (0)
	{}

private:

	bool OnInit ()
	try
	{
		wxInitAllImageHandlers ();

		Config::FailedToLoad.connect (boost::bind (&App::config_failed_to_load, this));
		Config::Warning.connect (boost::bind (&App::config_warning, this, _1));

		wxSplashScreen* splash = maybe_show_splash ();

		SetAppName (_("DCP-o-matic Player"));

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

		_frame = new DOMFrame ();
		SetTopWindow (_frame);
		_frame->Maximize ();
		if (splash) {
			splash->Destroy ();
		}
		_frame->Show ();

		signal_manager = new wxSignalManager (this);

		if (!_dcp_to_load.empty() && boost::filesystem::is_directory (_dcp_to_load)) {
			try {
				_frame->load_dcp (_dcp_to_load);
			} catch (exception& e) {
				error_dialog (0, std_to_wx (String::compose (wx_to_std (_("Could not load DCP %1.")), _dcp_to_load)), std_to_wx(e.what()));
			}
		}

		Bind (wxEVT_IDLE, boost::bind (&App::idle, this));

		if (Config::instance()->check_for_updates ()) {
			UpdateChecker::instance()->run ();
		}

		return true;
	}
	catch (exception& e)
	{
		error_dialog (0, _("DCP-o-matic Player could not start."), std_to_wx(e.what()));
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
			_dcp_to_load = wx_to_std (parser.GetParam (0));
		}

		return true;
	}

	void report_exception ()
	{
		try {
			throw;
		} catch (FileError& e) {
			error_dialog (
				0,
				wxString::Format (
					_("An exception occurred: %s (%s)\n\n") + REPORT_PROBLEM,
					std_to_wx (e.what()),
					std_to_wx (e.file().string().c_str ())
					)
				);
		} catch (exception& e) {
			error_dialog (
				0,
				wxString::Format (
					_("An exception occurred: %s.\n\n") + REPORT_PROBLEM,
					std_to_wx (e.what ())
					)
				);
		} catch (...) {
			error_dialog (0, _("An unknown exception occurred.") + "  " + REPORT_PROBLEM);
		}
	}

	/* An unhandled exception has occurred inside the main event loop */
	bool OnExceptionInMainLoop ()
	{
		report_exception ();
		/* This will terminate the program */
		return false;
	}

	void OnUnhandledException ()
	{
		report_exception ();
	}

	void idle ()
	{
		signal_manager->ui_idle ();
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
	string _dcp_to_load;
};

IMPLEMENT_APP (App)
