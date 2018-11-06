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

#include "wx/wx_signal_manager.h"
#include "wx/wx_util.h"
#include "wx/about_dialog.h"
#include "wx/report_problem_dialog.h"
#include "wx/film_viewer.h"
#include "wx/player_information.h"
#include "wx/update_dialog.h"
#include "wx/player_config_dialog.h"
#include "wx/verify_dcp_dialog.h"
#include "wx/controls.h"
#include "lib/cross.h"
#include "lib/config.h"
#include "lib/util.h"
#include "lib/internet.h"
#include "lib/update_checker.h"
#include "lib/compose.hpp"
#include "lib/dcp_content.h"
#include "lib/job_manager.h"
#include "lib/job.h"
#include "lib/film.h"
#include "lib/video_content.h"
#include "lib/text_content.h"
#include "lib/ratio.h"
#include "lib/verify_dcp_job.h"
#include "lib/dcp_examiner.h"
#include "lib/examine_content_job.h"
#include "lib/server.h"
#include "lib/dcpomatic_socket.h"
#include "lib/scoped_temporary.h"
#include "lib/monitor_checker.h"
#include <dcp/dcp.h>
#include <dcp/raw_convert.h>
#include <dcp/exceptions.h>
#include <wx/wx.h>
#include <wx/stdpaths.h>
#include <wx/splash.h>
#include <wx/cmdline.h>
#include <wx/preferences.h>
#include <wx/progdlg.h>
#include <wx/display.h>
#ifdef __WXOSX__
#include <ApplicationServices/ApplicationServices.h>
#endif
#include <boost/bind.hpp>
#include <iostream>

#ifdef check
#undef check
#endif

#define MAX_CPLS 32

using std::string;
using std::cout;
using std::list;
using std::exception;
using std::vector;
using boost::shared_ptr;
using boost::scoped_array;
using boost::optional;
using boost::dynamic_pointer_cast;
using boost::thread;
using boost::bind;

enum {
	ID_file_open = 1,
	ID_file_add_ov,
	ID_file_add_kdm,
	ID_file_history,
	/* Allow spare IDs after _history for the recent files list */
	ID_file_close = 100,
	ID_view_cpl,
	/* Allow spare IDs for CPLs */
	ID_view_full_screen = 200,
	ID_view_dual_screen,
	ID_view_closed_captions,
	ID_view_scale_appropriate,
	ID_view_scale_full,
	ID_view_scale_half,
	ID_view_scale_quarter,
	ID_help_report_a_problem,
	ID_tools_verify,
	ID_tools_check_for_updates,
	/* IDs for shortcuts (with no associated menu item) */
	ID_start_stop,
	ID_back_frame,
	ID_forward_frame
};

class DOMFrame : public wxFrame
{
public:
	DOMFrame ()
		: wxFrame (0, -1, _("DCP-o-matic Player"))
		, _dual_screen (0)
		, _update_news_requested (false)
		, _info (0)
		, _mode (Config::instance()->player_mode())
		, _config_dialog (0)
		, _file_menu (0)
		, _history_items (0)
		, _history_position (0)
		, _history_separator (0)
		, _view_full_screen (0)
		, _view_dual_screen (0)
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

		_config_changed_connection = Config::instance()->Changed.connect (boost::bind (&DOMFrame::config_changed, this, _1));
		update_from_config ();

		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_open, this), ID_file_open);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_add_ov, this), ID_file_add_ov);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_add_kdm, this), ID_file_add_kdm);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_history, this, _1), ID_file_history, ID_file_history + HISTORY_SIZE);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_close, this), ID_file_close);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_exit, this), wxID_EXIT);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::edit_preferences, this), wxID_PREFERENCES);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::view_full_screen, this), ID_view_full_screen);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::view_dual_screen, this), ID_view_dual_screen);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::view_closed_captions, this), ID_view_closed_captions);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::view_cpl, this, _1), ID_view_cpl, ID_view_cpl + MAX_CPLS);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::set_decode_reduction, this, optional<int>(0)), ID_view_scale_full);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::set_decode_reduction, this, optional<int>(1)), ID_view_scale_half);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::set_decode_reduction, this, optional<int>(2)), ID_view_scale_quarter);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::help_about, this), wxID_ABOUT);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::help_report_a_problem, this), ID_help_report_a_problem);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_verify, this), ID_tools_verify);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_check_for_updates, this), ID_tools_check_for_updates);

		/* Use a panel as the only child of the Frame so that we avoid
		   the dark-grey background on Windows.
		*/
		_overall_panel = new wxPanel (this, wxID_ANY);

		_viewer.reset (new FilmViewer (_overall_panel));
		_controls = new Controls (_overall_panel, _viewer, false);
		_viewer->set_dcp_decode_reduction (Config::instance()->decode_reduction ());
		_viewer->PlaybackPermitted.connect (bind(&DOMFrame::playback_permitted, this));
		_viewer->Started.connect (bind(&DOMFrame::playback_started, this, _1));
		_viewer->Seeked.connect (bind(&DOMFrame::playback_seeked, this, _1));
		_viewer->Stopped.connect (bind(&DOMFrame::playback_stopped, this, _1));
#ifdef DCPOMATIC_VARIANT_SWAROOP
		_viewer->PositionChanged.connect (bind(&DOMFrame::position_changed, this));
#endif
		_info = new PlayerInformation (_overall_panel, _viewer);
		setup_main_sizer (Config::instance()->player_mode());
#ifdef __WXOSX__
		int accelerators = 4;
#else
		int accelerators = 3;
#endif

		wxAcceleratorEntry* accel = new wxAcceleratorEntry[accelerators];
		accel[0].Set(wxACCEL_NORMAL, WXK_SPACE, ID_start_stop);
		accel[1].Set(wxACCEL_NORMAL, WXK_LEFT, ID_back_frame);
		accel[2].Set(wxACCEL_NORMAL, WXK_RIGHT, ID_forward_frame);
#ifdef __WXOSX__
		accel[3].Set(wxACCEL_CTRL, static_cast<int>('W'), ID_file_close);
#endif
		wxAcceleratorTable accel_table (accelerators, accel);
		SetAcceleratorTable (accel_table);
		delete[] accel;

		Bind (wxEVT_MENU, boost::bind (&DOMFrame::start_stop_pressed, this), ID_start_stop);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::back_frame, this), ID_back_frame);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::forward_frame, this), ID_forward_frame);

		reset_film ();

		UpdateChecker::instance()->StateChanged.connect (boost::bind (&DOMFrame::update_checker_state_changed, this));
#ifdef DCPOMATIC_VARIANT_SWAROOP
		MonitorChecker::instance()->StateChanged.connect(boost::bind(&DOMFrame::monitor_checker_state_changed, this));
		MonitorChecker::instance()->run ();
#endif
		setup_screen ();

#ifdef DCPOMATIC_VARIANT_SWAROOP
		if (
			boost::filesystem::is_regular_file(Config::path("position")) &&
			boost::filesystem::is_regular_file(Config::path("spl.xml"))) {

			shared_ptr<Film> film (new Film(boost::optional<boost::filesystem::path>()));
			film->read_metadata (Config::path("spl.xml"));
			reset_film (film);
			FILE* f = fopen_boost (Config::path("position"), "r");
			if (f) {
				char buffer[64];
				fscanf (f, "%63s", buffer);
				_viewer->seek (DCPTime(atoi(buffer)), true);
				_viewer->start ();
				fclose (f);
			}
		}

#endif
	}

	void position_changed ()
	{
		if (!_viewer->playing() || _viewer->position().get() % DCPTime::HZ) {
			return;
		}

		FILE* f = fopen_boost (Config::path("position"), "w");
		if (f) {
			string const p = dcp::raw_convert<string> (_viewer->position().get());
			fwrite (p.c_str(), p.length(), 1, f);
			fclose (f);
		}
	}

#ifdef DCPOMATIC_VARIANT_SWAROOP
	void monitor_checker_state_changed ()
	{
		if (!MonitorChecker::instance()->ok()) {
			error_dialog (this, _("The required display devices are not connected correctly."));
			_viewer->stop ();
		}
	}
#endif

	void setup_main_sizer (Config::PlayerMode mode)
	{
		wxSizer* main_sizer = new wxBoxSizer (wxVERTICAL);
		if (mode != Config::PLAYER_MODE_DUAL) {
			main_sizer->Add (_viewer->panel(), 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);
		}
		main_sizer->Add (_controls, mode == Config::PLAYER_MODE_DUAL ? 1 : 0, wxEXPAND | wxALL, 6);
		main_sizer->Add (_info, 0, wxEXPAND | wxALL, 6);
		_overall_panel->SetSizer (main_sizer);
		_overall_panel->Layout ();
	}

	bool playback_permitted ()
	{
#ifdef DCPOMATIC_VARIANT_SWAROOP
		if (!MonitorChecker::instance()->ok()) {
			error_dialog (this, _("The required display devices are not connected correctly."));
			return false;
		}
#endif
		if (!_film || !Config::instance()->respect_kdm_validity_periods()) {
			return true;
		}

		bool ok = true;
		BOOST_FOREACH (shared_ptr<Content> i, _film->content()) {
			shared_ptr<DCPContent> d = dynamic_pointer_cast<DCPContent>(i);
			if (d && !d->kdm_timing_window_valid()) {
				ok = false;
			}
		}

		if (!ok) {
			error_dialog (this, _("The KDM does not allow playback of this content at this time."));
		}

		return ok;
	}

	void playback_started (DCPTime time)
	{
		optional<boost::filesystem::path> log = Config::instance()->player_log_file();
		if (!log) {
			return;
		}

		shared_ptr<DCPContent> dcp = boost::dynamic_pointer_cast<DCPContent>(_film->content().front());
		DCPOMATIC_ASSERT (dcp);
		DCPExaminer ex (dcp);
		shared_ptr<dcp::CPL> playing_cpl;
		BOOST_FOREACH (shared_ptr<dcp::CPL> i, ex.cpls()) {
			if (!dcp->cpl() || i->id() == *dcp->cpl()) {
				playing_cpl = i;
			}
		}
		DCPOMATIC_ASSERT (playing_cpl)

		FILE* f = fopen_boost(*log, "a");
		fprintf (
			f,
			"%s playback-started %s %s %s\n",
			dcp::LocalTime().as_string().c_str(),
			time.timecode(_film->video_frame_rate()).c_str(),
			dcp->directories().front().string().c_str(),
			playing_cpl->annotation_text().c_str()
			);
		fclose (f);
	}

	void playback_seeked (DCPTime time)
	{
		optional<boost::filesystem::path> log = Config::instance()->player_log_file();
		if (!log) {
			return;
		}

		FILE* f = fopen_boost(*log, "a");
		fprintf (f, "%s playback-seeked %s\n", dcp::LocalTime().as_string().c_str(), time.timecode(_film->video_frame_rate()).c_str());
		fclose (f);
	}

	void playback_stopped (DCPTime time)
	{
#ifdef DCPOMATIC_VARIANT_SWAROOP
		try {
			boost::filesystem::remove (Config::path("position"));
		} catch (...) {
			/* Never mind */
		}
#endif

		optional<boost::filesystem::path> log = Config::instance()->player_log_file();
		if (!log) {
			return;
		}

		FILE* f = fopen_boost(*log, "a");
		fprintf (f, "%s playback-stopped %s\n", dcp::LocalTime().as_string().c_str(), time.timecode(_film->video_frame_rate()).c_str());
		fclose (f);
	}

	void set_decode_reduction (optional<int> reduction)
	{
		_viewer->set_dcp_decode_reduction (reduction);
		_info->triggered_update ();
		Config::instance()->set_decode_reduction (reduction);
	}

	void load_dcp (boost::filesystem::path dir)
	{
		DCPOMATIC_ASSERT (_film);

		reset_film ();
		try {
			shared_ptr<DCPContent> dcp (new DCPContent(_film, dir));
			_film->examine_and_add_content (dcp);
			bool const ok = display_progress (_("DCP-o-matic Player"), _("Loading content"));
			if (!ok || !report_errors_from_last_job(this)) {
				return;
			}
			Config::instance()->add_to_player_history (dir);
		} catch (dcp::DCPReadError& e) {
			error_dialog (this, wxString::Format(_("Could not load a DCP from %s"), std_to_wx(dir.string())), std_to_wx(e.what()));
		}
	}

#ifdef DCPOMATIC_VARIANT_SWAROOP
	optional<dcp::EncryptedKDM> get_kdm_from_url (shared_ptr<DCPContent> dcp)
	{
		ScopedTemporary temp;
		string url = Config::instance()->kdm_server_url();
		boost::algorithm::replace_all (url, "{CPL}", *dcp->cpl());
		optional<dcp::EncryptedKDM> kdm;
		if (dcp->cpl() && !get_from_url(url, false, temp)) {
			try {
				kdm = dcp::EncryptedKDM (dcp::file_to_string(temp.file()));
				if (kdm->cpl_id() != dcp->cpl()) {
					kdm = boost::none;
				}
			} catch (std::exception& e) {
				/* Hey well */
			}
		}
		return kdm;
	}
#endif

	optional<dcp::EncryptedKDM> get_kdm_from_directory (shared_ptr<DCPContent> dcp)
	{
		using namespace boost::filesystem;
		optional<path> kdm_dir = Config::instance()->player_kdm_directory();
		if (!kdm_dir) {
			return optional<dcp::EncryptedKDM>();
		}
		for (directory_iterator i = directory_iterator(*kdm_dir); i != directory_iterator(); ++i) {
			try {
				if (file_size(i->path()) < MAX_KDM_SIZE) {
					dcp::EncryptedKDM kdm (dcp::file_to_string(i->path()));
					if (kdm.cpl_id() == dcp->cpl()) {
						return kdm;
					}
				}
			} catch (std::exception& e) {
				/* Hey well */
			}
		}
		return optional<dcp::EncryptedKDM>();
	}

	void reset_film (shared_ptr<Film> film = shared_ptr<Film>(new Film(optional<boost::filesystem::path>())))
	{
		_film = film;
		_viewer->set_film (_film);
		_film->Change.connect (bind(&DOMFrame::film_changed, this, _1, _2));
	}

	void film_changed (ChangeType type, Film::Property property)
	{
		if (type != CHANGE_TYPE_DONE || property != Film::CONTENT) {
			return;
		}

		_film->write_metadata (Config::path("spl.xml"));

		if (_viewer->playing ()) {
			_viewer->stop ();
		}

		/* Start off as Flat */
		_film->set_container (Ratio::from_id("185"));

		BOOST_FOREACH (shared_ptr<Content> i, _film->content()) {
			shared_ptr<DCPContent> dcp = dynamic_pointer_cast<DCPContent>(i);
			if (dcp && dcp->needs_kdm()) {
				optional<dcp::EncryptedKDM> kdm;
#ifdef DCPOMATIC_VARIANT_SWAROOP
				kdm = get_kdm_from_url (dcp);
#endif
				if (!kdm) {
					get_kdm_from_directory (dcp);
				}

				if (kdm) {
					dcp->add_kdm (*kdm);
					dcp->examine (shared_ptr<Job>());
				}
			}

			BOOST_FOREACH (shared_ptr<TextContent> j, i->text) {
				j->set_use (true);
			}

			if (i->video) {
				Ratio const * r = Ratio::nearest_from_ratio(i->video->size().ratio());
				if (r->id() == "239") {
					/* Any scope content means we use scope */
					_film->set_container(r);
				}
			}

			/* Any 3D content means we use 3D mode */
			if (i->video && i->video->frame_type() != VIDEO_FRAME_TYPE_2D) {
				_film->set_three_d (true);
			}
		}

		_viewer->seek (DCPTime(), true);
		_info->triggered_update ();

		set_menu_sensitivity ();

		wxMenuItemList old = _cpl_menu->GetMenuItems();
		for (wxMenuItemList::iterator i = old.begin(); i != old.end(); ++i) {
			_cpl_menu->Remove (*i);
		}

		if (_film->content().size() == 1) {
			/* Offer a CPL menu */
			shared_ptr<DCPContent> first = dynamic_pointer_cast<DCPContent>(_film->content().front());
			if (first) {
				DCPExaminer ex (first);
				int id = ID_view_cpl;
				BOOST_FOREACH (shared_ptr<dcp::CPL> i, ex.cpls()) {
					wxMenuItem* j = _cpl_menu->AppendRadioItem(
						id,
						wxString::Format("%s (%s)", std_to_wx(i->annotation_text()).data(), std_to_wx(i->id()).data())
						);
					j->Check(!first->cpl() || i->id() == *first->cpl());
					++id;
				}
			}
		}
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
		wxMenuItem* prefs = _file_menu->Append (wxID_PREFERENCES, _("&Preferences...\tCtrl-P"));
#else
		wxMenu* edit = new wxMenu;
		wxMenuItem* prefs = edit->Append (wxID_PREFERENCES, _("&Preferences...\tCtrl-P"));
#endif

		prefs->Enable (Config::instance()->have_write_permission());

		_cpl_menu = new wxMenu;

		wxMenu* view = new wxMenu;
		optional<int> c = Config::instance()->decode_reduction();
		_view_cpl = view->Append(ID_view_cpl, _("CPL"), _cpl_menu);
		view->AppendSeparator();
#ifndef DCPOMATIC_VARIANT_SWAROOP
		_view_full_screen = view->AppendCheckItem(ID_view_full_screen, _("Full screen\tF11"));
		_view_dual_screen = view->AppendCheckItem(ID_view_dual_screen, _("Dual screen\tShift+F11"));
#endif
		setup_menu ();
		view->AppendSeparator();
		view->Append(ID_view_closed_captions, _("Closed captions..."));
		view->AppendSeparator();
		view->AppendRadioItem(ID_view_scale_appropriate, _("Set decode resolution to match display"))->Check(!static_cast<bool>(c));
		view->AppendRadioItem(ID_view_scale_full, _("Decode at full resolution"))->Check(c && c.get() == 0);
		view->AppendRadioItem(ID_view_scale_half, _("Decode at half resolution"))->Check(c && c.get() == 1);
		view->AppendRadioItem(ID_view_scale_quarter, _("Decode at quarter resolution"))->Check(c && c.get() == 2);

		wxMenu* tools = new wxMenu;
		_tools_verify = tools->Append (ID_tools_verify, _("Verify DCP"));
		tools->AppendSeparator ();
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
			JobManager::instance()->add(shared_ptr<Job>(new ExamineContentJob (_film, dcp)));
			bool const ok = display_progress (_("DCP-o-matic Player"), _("Loading content"));
			if (!ok || !report_errors_from_last_job(this)) {
				return;
			}
			BOOST_FOREACH (shared_ptr<TextContent> i, dcp->text) {
				i->set_use (true);
			}
			if (dcp->video) {
				Ratio const * r = Ratio::nearest_from_ratio(dcp->video->size().ratio());
				if (r) {
					_film->set_container(r);
				}
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
		reset_film ();
		_info->triggered_update ();
		set_menu_sensitivity ();
	}

	void file_exit ()
	{
		Close ();
	}

	void edit_preferences ()
	{
		if (!Config::instance()->have_write_permission()) {
			return;
		}

		if (!_config_dialog) {
			_config_dialog = create_player_config_dialog ();
		}
		_config_dialog->Show (this);
	}

	void view_cpl (wxCommandEvent& ev)
	{
		shared_ptr<DCPContent> dcp = boost::dynamic_pointer_cast<DCPContent>(_film->content().front());
		DCPOMATIC_ASSERT (dcp);
		DCPExaminer ex (dcp);
		int id = ev.GetId() - ID_view_cpl;
		DCPOMATIC_ASSERT (id >= 0);
		DCPOMATIC_ASSERT (id < int(ex.cpls().size()));
		list<shared_ptr<dcp::CPL> > cpls = ex.cpls();
		list<shared_ptr<dcp::CPL> >::iterator i = cpls.begin();
		while (id > 0) {
			++i;
			--id;
		}

		dcp->set_cpl ((*i)->id());
		dcp->examine (shared_ptr<Job>());
	}

	void view_full_screen ()
	{
		if (_mode == Config::PLAYER_MODE_FULL) {
			_mode = Config::PLAYER_MODE_WINDOW;
		} else {
			_mode = Config::PLAYER_MODE_FULL;
		}
		setup_screen ();
		setup_menu ();
	}

	void view_dual_screen ()
	{
		if (_mode == Config::PLAYER_MODE_DUAL) {
			_mode = Config::PLAYER_MODE_WINDOW;
		} else {
			_mode = Config::PLAYER_MODE_DUAL;
		}
		setup_screen ();
		setup_menu ();
	}

	void setup_menu ()
	{
		if (_view_full_screen) {
			_view_full_screen->Check (_mode == Config::PLAYER_MODE_FULL);
		}
		if (_view_dual_screen) {
			_view_dual_screen->Check (_mode == Config::PLAYER_MODE_DUAL);
		}
	}

	void setup_screen ()
	{
		_controls->Show (_mode != Config::PLAYER_MODE_FULL);
		_controls->show_extended_player_controls (_mode == Config::PLAYER_MODE_DUAL);
		_info->Show (_mode != Config::PLAYER_MODE_FULL);
		_overall_panel->SetBackgroundColour (_mode == Config::PLAYER_MODE_FULL ? wxColour(0, 0, 0) : wxNullColour);
		ShowFullScreen (_mode == Config::PLAYER_MODE_FULL);

		if (_mode == Config::PLAYER_MODE_DUAL) {
			_dual_screen = new wxFrame (this, wxID_ANY, wxT(""));
			_dual_screen->SetBackgroundColour (wxColour(0, 0, 0));
			_dual_screen->ShowFullScreen (true);
			_viewer->panel()->Reparent (_dual_screen);
			_dual_screen->Show ();
			if (wxDisplay::GetCount() > 1) {
				switch (Config::instance()->image_display()) {
				case 0:
					_dual_screen->Move (0, 0);
					Move (wxDisplay(0).GetClientArea().GetWidth(), 0);
					break;
				case 1:
					_dual_screen->Move (wxDisplay(0).GetClientArea().GetWidth(), 0);
					// (0, 0) doesn't seem to work for some strange reason
					Move (8, 8);
					break;
				}
			}
		} else {
			if (_dual_screen) {
				_viewer->panel()->Reparent (_overall_panel);
				_dual_screen->Destroy ();
				_dual_screen = 0;
			}
		}

		setup_main_sizer (_mode);
	}

	void view_closed_captions ()
	{
		_viewer->show_closed_captions ();
	}

	void tools_verify ()
	{
		shared_ptr<DCPContent> dcp = boost::dynamic_pointer_cast<DCPContent>(_film->content().front());
		DCPOMATIC_ASSERT (dcp);

		JobManager* jm = JobManager::instance ();
		jm->add (shared_ptr<Job> (new VerifyDCPJob (dcp->directories())));
		bool const ok = display_progress (_("DCP-o-matic Player"), _("Verifying DCP"));
		if (!ok) {
			return;
		}

		DCPOMATIC_ASSERT (!jm->get().empty());
		shared_ptr<VerifyDCPJob> last = dynamic_pointer_cast<VerifyDCPJob> (jm->get().back());
		DCPOMATIC_ASSERT (last);

		VerifyDCPDialog* d = new VerifyDCPDialog (this, last);
		d->ShowModal ();
		d->Destroy ();
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

	void config_changed (Config::Property prop)
	{
		/* Instantly save any config changes when using the player GUI */
		try {
			Config::instance()->write_config();
		} catch (FileError& e) {
			if (prop != Config::HISTORY) {
				error_dialog (
					this,
					wxString::Format(
						_("Could not write to config file at %s.  Your changes have not been saved."),
						std_to_wx(e.file().string())
						)
					);
			}
		} catch (exception& e) {
			error_dialog (
				this,
				_("Could not write to config file.  Your changes have not been saved.")
				);
		}

		update_from_config ();
	}

	void update_from_config ()
	{
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
		_tools_verify->Enable (static_cast<bool>(_film));
		_file_add_ov->Enable (static_cast<bool>(_film));
		_file_add_kdm->Enable (static_cast<bool>(_film));
		_view_cpl->Enable (static_cast<bool>(_film));
	}

	void start_stop_pressed ()
	{
		if (_viewer->playing()) {
			_viewer->stop();
		} else {
			_viewer->start();
		}
	}

	void back_frame ()
	{
		_viewer->seek_by (-_viewer->one_video_frame(), true);
	}

	void forward_frame ()
	{
		_viewer->seek_by (_viewer->one_video_frame(), true);
	}

private:

	wxFrame* _dual_screen;
	bool _update_news_requested;
	PlayerInformation* _info;
	Config::PlayerMode _mode;
	wxPreferencesEditor* _config_dialog;
	wxPanel* _overall_panel;
	wxMenu* _file_menu;
	wxMenuItem* _view_cpl;
	wxMenu* _cpl_menu;
	int _history_items;
	int _history_position;
	wxMenuItem* _history_separator;
	shared_ptr<FilmViewer> _viewer;
	Controls* _controls;
	boost::shared_ptr<Film> _film;
	boost::signals2::scoped_connection _config_changed_connection;
	wxMenuItem* _file_add_ov;
	wxMenuItem* _file_add_kdm;
	wxMenuItem* _tools_verify;
	wxMenuItem* _view_full_screen;
	wxMenuItem* _view_dual_screen;
};

static const wxCmdLineEntryDesc command_line_description[] = {
	{ wxCMD_LINE_PARAM, 0, 0, "DCP to load or create", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_NONE, "", "", "", wxCmdLineParamType (0), 0 }
};

class PlayServer : public Server
{
public:
	explicit PlayServer (DOMFrame* frame)
		: Server (PLAYER_PLAY_PORT)
		, _frame (frame)
	{}

	void handle (shared_ptr<Socket> socket)
	{
		try {
			int const length = socket->read_uint32 ();
			scoped_array<char> buffer (new char[length]);
			socket->read (reinterpret_cast<uint8_t*> (buffer.get()), length);
			string s (buffer.get());
			signal_manager->when_idle (bind (&DOMFrame::load_dcp, _frame, s));
			socket->write (reinterpret_cast<uint8_t const *> ("OK"), 3);
		} catch (...) {

		}
	}

private:
	DOMFrame* _frame;
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

		signal_manager = new wxSignalManager (this);

		_frame = new DOMFrame ();
		SetTopWindow (_frame);
		_frame->Maximize ();
		if (splash) {
			splash->Destroy ();
		}
		_frame->Show ();

		PlayServer* server = new PlayServer (_frame);
		new thread (boost::bind (&PlayServer::run, server));

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
