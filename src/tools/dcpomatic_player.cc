/*
    Copyright (C) 2017-2020 Carl Hetherington <cth@carlh.net>

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
#include "wx/standard_controls.h"
#include "wx/playlist_controls.h"
#ifdef DCPOMATIC_VARIANT_SWAROOP
#include "wx/swaroop_controls.h"
#endif
#include "wx/timer_display.h"
#include "wx/system_information_dialog.h"
#include "wx/player_stress_tester.h"
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
#include "lib/null_log.h"
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
#include "lib/lock_file_checker.h"
#include "lib/ffmpeg_content.h"
#include "lib/dcpomatic_log.h"
#include "lib/file_log.h"
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
#ifdef __WXGTK__
#include <X11/Xlib.h>
#endif
#ifdef __WXOSX__
#include <ApplicationServices/ApplicationServices.h>
#endif
#include <boost/bind.hpp>
#include <boost/algorithm/string.hpp>
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
using boost::weak_ptr;
using boost::scoped_array;
using boost::optional;
using boost::dynamic_pointer_cast;
using boost::thread;
using boost::bind;
using dcp::raw_convert;
using namespace dcpomatic;

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
	ID_tools_timing,
	ID_tools_system_information,
	/* IDs for shortcuts (with no associated menu item) */
	ID_start_stop,
	ID_go_back_frame,
	ID_go_forward_frame,
	ID_go_back_small_amount,
	ID_go_forward_small_amount,
	ID_go_back_medium_amount,
	ID_go_forward_medium_amount,
	ID_go_back_large_amount,
	ID_go_forward_large_amount,
	ID_go_to_start,
	ID_go_to_end
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
		, _system_information_dialog (0)
		, _view_full_screen (0)
		, _view_dual_screen (0)
{
		dcpomatic_log.reset (new NullLog());

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
		update_from_config (Config::PLAYER_DEBUG_LOG);

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
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_timing, this), ID_tools_timing);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_system_information, this), ID_tools_system_information);

		/* Use a panel as the only child of the Frame so that we avoid
		   the dark-grey background on Windows.
		*/
		_overall_panel = new wxPanel (this, wxID_ANY);

		_viewer.reset (new FilmViewer (_overall_panel));
#ifdef DCPOMATIC_VARIANT_SWAROOP
		SwaroopControls* sc = new SwaroopControls (_overall_panel, _viewer);
		_controls = sc;
		sc->ResetFilm.connect (bind(&DOMFrame::reset_film_weak, this, _1));
#else
		if (Config::instance()->player_mode() == Config::PLAYER_MODE_DUAL) {
			PlaylistControls* pc = new PlaylistControls (_overall_panel, _viewer);
			_controls = pc;
			pc->ResetFilm.connect (bind(&DOMFrame::reset_film_weak, this, _1));
		} else {
			_controls = new StandardControls (_overall_panel, _viewer, false);
		}
#endif
		_viewer->set_dcp_decode_reduction (Config::instance()->decode_reduction ());
		_viewer->PlaybackPermitted.connect (bind(&DOMFrame::playback_permitted, this));
		_viewer->Started.connect (bind(&DOMFrame::playback_started, this, _1));
		_viewer->Stopped.connect (bind(&DOMFrame::playback_stopped, this, _1));
		_info = new PlayerInformation (_overall_panel, _viewer);
		setup_main_sizer (Config::instance()->player_mode());
#ifdef __WXOSX__
		int accelerators = 12;
#else
		int accelerators = 11;
#endif

		_stress.setup (this, _controls);

		wxAcceleratorEntry* accel = new wxAcceleratorEntry[accelerators];
		accel[0].Set(wxACCEL_NORMAL,                WXK_SPACE, ID_start_stop);
		accel[1].Set(wxACCEL_NORMAL,                WXK_LEFT,  ID_go_back_frame);
		accel[2].Set(wxACCEL_NORMAL,                WXK_RIGHT, ID_go_forward_frame);
		accel[3].Set(wxACCEL_SHIFT,                 WXK_LEFT,  ID_go_back_small_amount);
		accel[4].Set(wxACCEL_SHIFT,                 WXK_RIGHT, ID_go_forward_small_amount);
		accel[5].Set(wxACCEL_CTRL,                  WXK_LEFT,  ID_go_back_medium_amount);
		accel[6].Set(wxACCEL_CTRL,                  WXK_RIGHT, ID_go_forward_medium_amount);
		accel[7].Set(wxACCEL_SHIFT | wxACCEL_CTRL,  WXK_LEFT,  ID_go_back_large_amount);
		accel[8].Set(wxACCEL_SHIFT | wxACCEL_CTRL,  WXK_RIGHT, ID_go_forward_large_amount);
		accel[9].Set(wxACCEL_NORMAL,                WXK_HOME,  ID_go_to_start);
		accel[10].Set(wxACCEL_NORMAL,               WXK_END,   ID_go_to_end);
#ifdef __WXOSX__
		accel[11].Set(wxACCEL_CTRL, static_cast<int>('W'), ID_file_close);
#endif
		wxAcceleratorTable accel_table (accelerators, accel);
		SetAcceleratorTable (accel_table);
		delete[] accel;

		Bind (wxEVT_MENU, boost::bind(&DOMFrame::start_stop_pressed, this), ID_start_stop);
		Bind (wxEVT_MENU, boost::bind(&DOMFrame::go_back_frame, this),      ID_go_back_frame);
		Bind (wxEVT_MENU, boost::bind(&DOMFrame::go_forward_frame, this),   ID_go_forward_frame);
		Bind (wxEVT_MENU, boost::bind(&DOMFrame::go_seconds,  this,   -60), ID_go_back_small_amount);
		Bind (wxEVT_MENU, boost::bind(&DOMFrame::go_seconds,  this,    60), ID_go_forward_small_amount);
		Bind (wxEVT_MENU, boost::bind(&DOMFrame::go_seconds,  this,  -600), ID_go_back_medium_amount);
		Bind (wxEVT_MENU, boost::bind(&DOMFrame::go_seconds,  this,   600), ID_go_forward_medium_amount);
		Bind (wxEVT_MENU, boost::bind(&DOMFrame::go_seconds,  this, -3600), ID_go_back_large_amount);
		Bind (wxEVT_MENU, boost::bind(&DOMFrame::go_seconds,  this,  3600), ID_go_forward_large_amount);
		Bind (wxEVT_MENU, boost::bind(&DOMFrame::go_to_start, this), ID_go_to_start);
		Bind (wxEVT_MENU, boost::bind(&DOMFrame::go_to_end,   this), ID_go_to_end);

		reset_film ();

		UpdateChecker::instance()->StateChanged.connect (boost::bind (&DOMFrame::update_checker_state_changed, this));
#ifdef DCPOMATIC_VARIANT_SWAROOP
		MonitorChecker::instance()->StateChanged.connect(boost::bind(&DOMFrame::monitor_checker_state_changed, this));
		MonitorChecker::instance()->run ();
		LockFileChecker::instance()->StateChanged.connect(boost::bind(&DOMFrame::lock_checker_state_changed, this));
		LockFileChecker::instance()->run ();
#endif
		setup_screen ();

		_stress.LoadDCP.connect (boost::bind(&DOMFrame::load_dcp, this, _1));

#ifdef DCPOMATIC_VARIANT_SWAROOP
		sc->check_restart ();
#endif
	}

#ifdef DCPOMATIC_VARIANT_SWAROOP
	void monitor_checker_state_changed ()
	{
		if (!MonitorChecker::instance()->ok()) {
			_viewer->stop ();
			error_dialog (this, _("The required display devices are not connected correctly."));
		}
	}

	void lock_checker_state_changed ()
	{
		if (!LockFileChecker::instance()->ok()) {
			_viewer->stop ();
			error_dialog (this, _("The lock file is not present."));
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
		if (!LockFileChecker::instance()->ok()) {
			error_dialog (this, _("The lock file is not present."));
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

#ifdef DCPOMATIC_VARIANT_SWAROOP
		BOOST_FOREACH (shared_ptr<Content> i, _film->content()) {
			shared_ptr<FFmpegContent> c = dynamic_pointer_cast<FFmpegContent>(i);
			if (c && !c->kdm_timing_window_valid()) {
				ok = false;
			}
		}
#endif

		if (!ok) {
			error_dialog (this, _("The KDM does not allow playback of this content at this time."));
		}

		return ok;
	}

	void playback_started (DCPTime time)
	{
		/* XXX: this only logs the first piece of content; probably should be each piece? */
		if (_film->content().empty()) {
			return;
		}

		shared_ptr<DCPContent> dcp = dynamic_pointer_cast<DCPContent>(_film->content().front());
		if (dcp) {
			DCPExaminer ex (dcp, true);
			shared_ptr<dcp::CPL> playing_cpl;
			BOOST_FOREACH (shared_ptr<dcp::CPL> i, ex.cpls()) {
				if (!dcp->cpl() || i->id() == *dcp->cpl()) {
					playing_cpl = i;
				}
			}
			DCPOMATIC_ASSERT (playing_cpl);

			_controls->log (
				wxString::Format(
					"playback-started %s %s %s",
					time.timecode(_film->video_frame_rate()).c_str(),
					dcp->directories().front().string().c_str(),
					playing_cpl->annotation_text().c_str()
					)
				);
		}

		shared_ptr<FFmpegContent> ffmpeg = dynamic_pointer_cast<FFmpegContent>(_film->content().front());
		if (ffmpeg) {
			_controls->log (
				wxString::Format(
					"playback-started %s %s",
					time.timecode(_film->video_frame_rate()).c_str(),
					ffmpeg->path(0).string().c_str()
					)
				);
		}
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

		_controls->log (wxString::Format("playback-stopped %s", time.timecode(_film->video_frame_rate()).c_str()));
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
			_stress.set_suspended (true);
			shared_ptr<DCPContent> dcp (new DCPContent(dir));
			shared_ptr<Job> job (new ExamineContentJob(_film, dcp));
			_examine_job_connection = job->Finished.connect(bind(&DOMFrame::add_dcp_to_film, this, weak_ptr<Job>(job), weak_ptr<Content>(dcp)));
			JobManager::instance()->add (job);
			bool const ok = display_progress (_("DCP-o-matic Player"), _("Loading content"));
			if (!ok || !report_errors_from_last_job(this)) {
				return;
			}
#ifndef DCPOMATIC_VARIANT_SWAROOP
			Config::instance()->add_to_player_history (dir);
#endif
		} catch (dcp::ReadError& e) {
			error_dialog (this, wxString::Format(_("Could not load a DCP from %s"), std_to_wx(dir.string())), std_to_wx(e.what()));
		}
	}

	void add_dcp_to_film (weak_ptr<Job> weak_job, weak_ptr<Content> weak_content)
	{
		shared_ptr<Job> job = weak_job.lock ();
		if (!job || !job->finished_ok()) {
			return;
		}

		shared_ptr<Content> content = weak_content.lock ();
		if (!content) {
			return;
		}

		_film->add_content (content);
		_stress.set_suspended (false);
	}

	void reset_film_weak (weak_ptr<Film> weak_film)
	{
		shared_ptr<Film> film = weak_film.lock ();
		if (film) {
			reset_film (film);
		}
	}

	void reset_film (shared_ptr<Film> film = shared_ptr<Film>(new Film(optional<boost::filesystem::path>())))
	{
		_film = film;
		_film->set_tolerant (true);
		_film->set_audio_channels (MAX_DCP_AUDIO_CHANNELS);
		_viewer->set_film (_film);
		_controls->set_film (_film);
		_film->Change.connect (bind(&DOMFrame::film_changed, this, _1, _2));
		_info->triggered_update ();
	}

	void film_changed (ChangeType type, Film::Property property)
	{
		if (type != CHANGE_TYPE_DONE || property != Film::CONTENT) {
			return;
		}

		if (_viewer->playing ()) {
			_viewer->stop ();
		}

		/* Start off as Flat */
		_film->set_container (Ratio::from_id("185"));

		BOOST_FOREACH (shared_ptr<Content> i, _film->content()) {
			shared_ptr<DCPContent> dcp = dynamic_pointer_cast<DCPContent>(i);

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
				DCPExaminer ex (first, true);
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

	void load_stress_script (boost::filesystem::path path)
	{
		_stress.load_script (path);
	}

private:

	bool report_errors_from_last_job (wxWindow* parent) const
	{
		JobManager* jm = JobManager::instance ();

		DCPOMATIC_ASSERT (!jm->get().empty());

		shared_ptr<Job> last = jm->get().back();
		if (last->finished_in_error()) {
			error_dialog(parent, wxString::Format(_("Could not load DCP.\n\n%s."), std_to_wx(last->error_summary()).data()), std_to_wx(last->error_details()));
			return false;
		}

		return true;
	}

	void setup_menu (wxMenuBar* m)
	{
		_file_menu = new wxMenu;
		_file_menu->Append (ID_file_open, _("&Open...\tCtrl-O"));
		_file_add_ov = _file_menu->Append (ID_file_add_ov, _("&Add OV..."));
		_file_add_kdm = _file_menu->Append (ID_file_add_kdm, _("Add &KDM..."));

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
		tools->Append (ID_tools_timing, _("Timing..."));
		tools->Append (ID_tools_system_information, _("System information..."));

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
#ifdef DCPOMATIC_VARIANT_SWAROOP
			shared_ptr<FFmpegContent> ffmpeg = boost::dynamic_pointer_cast<FFmpegContent>(_film->content().front());
			if (ffmpeg) {
				try {
					ffmpeg->add_kdm (EncryptedECinemaKDM(dcp::file_to_string(wx_to_std(d->GetPath()), MAX_KDM_SIZE)));
				} catch (exception& e) {
					error_dialog (this, wxString::Format(_("Could not load KDM.")), std_to_wx(e.what()));
					d->Destroy();
					return;
				}
			}
#endif
			shared_ptr<DCPContent> dcp = boost::dynamic_pointer_cast<DCPContent>(_film->content().front());
#ifndef DCPOMATIC_VARIANT_SWAROOP
			DCPOMATIC_ASSERT (dcp);
#endif
			try {
				if (dcp) {
					dcp->add_kdm (dcp::EncryptedKDM(dcp::file_to_string(wx_to_std(d->GetPath()), MAX_KDM_SIZE)));
					dcp->examine (_film, shared_ptr<Job>());
				}
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
			try {
				load_dcp (history[n]);
			} catch (exception& e) {
				error_dialog (0, std_to_wx(String::compose(wx_to_std(_("Could not load DCP %1.")), history[n])), std_to_wx(e.what()));
			}
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
		DCPExaminer ex (dcp, true);
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
		dcp->examine (_film, shared_ptr<Job>());
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
		_info->Show (_mode != Config::PLAYER_MODE_FULL);
		_overall_panel->SetBackgroundColour (_mode == Config::PLAYER_MODE_FULL ? wxColour(0, 0, 0) : wxNullColour);
		ShowFullScreen (_mode == Config::PLAYER_MODE_FULL);
		_viewer->set_pad_black (_mode != Config::PLAYER_MODE_WINDOW);

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

	void tools_timing ()
	{
		TimerDisplay* d = new TimerDisplay (this, _viewer->state_timer(), _viewer->gets());
		d->ShowModal ();
		d->Destroy ();
	}

	void tools_system_information ()
	{
		if (!_system_information_dialog) {
			_system_information_dialog = new SystemInformationDialog (this, _viewer);
		}

		_system_information_dialog->Show ();
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

		update_from_config (prop);
	}

	void update_from_config (Config::Property prop)
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

		/* Clear out non-existant history items before we re-build the menu */
		Config::instance()->clean_player_history ();
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

		if (prop == Config::PLAYER_DEBUG_LOG) {
			optional<boost::filesystem::path> p = Config::instance()->player_debug_log_file();
			if (p) {
				dcpomatic_log.reset (new FileLog(*p));
			} else {
				dcpomatic_log.reset (new NullLog());
			}
			dcpomatic_log->set_types (LogEntry::TYPE_GENERAL | LogEntry::TYPE_WARNING | LogEntry::TYPE_ERROR | LogEntry::TYPE_DEBUG_PLAYER);
		}
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

	void go_back_frame ()
	{
		_viewer->seek_by (-_viewer->one_video_frame(), true);
	}

	void go_forward_frame ()
	{
		_viewer->seek_by (_viewer->one_video_frame(), true);
	}

	void go_seconds (int s)
	{
		_viewer->seek_by (DCPTime::from_seconds(s), true);
	}

	void go_to_start ()
	{
		_viewer->seek (DCPTime(), true);
	}

	void go_to_end ()
	{
		_viewer->seek (_film->length() - _viewer->one_video_frame(), true);
	}

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
	SystemInformationDialog* _system_information_dialog;
	boost::shared_ptr<Film> _film;
	boost::signals2::scoped_connection _config_changed_connection;
	boost::signals2::scoped_connection _examine_job_connection;
	wxMenuItem* _file_add_ov;
	wxMenuItem* _file_add_kdm;
	wxMenuItem* _tools_verify;
	wxMenuItem* _view_full_screen;
	wxMenuItem* _view_dual_screen;
	PlayerStressTester _stress;
};

static const wxCmdLineEntryDesc command_line_description[] = {
	{ wxCMD_LINE_PARAM, 0, 0, "DCP to load or create", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_OPTION, "c", "config", "Directory containing config.xml", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_OPTION, "s", "stress", "File containing description of stress test", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
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
	{
#ifdef DCPOMATIC_LINUX
		XInitThreads ();
#endif
	}

private:

	bool OnInit ()
	{
		wxSplashScreen* splash = 0;
		try {
			wxInitAllImageHandlers ();

			Config::FailedToLoad.connect (boost::bind (&App::config_failed_to_load, this));
			Config::Warning.connect (boost::bind (&App::config_warning, this, _1));

			splash = maybe_show_splash ();

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
				splash = 0;
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

			if (_stress) {
				try {
					_frame->load_stress_script (*_stress);
				} catch (exception& e) {
					error_dialog (0, wxString::Format("Could not load stress test file %s", std_to_wx(*_stress)));
				}
			}

			Bind (wxEVT_IDLE, boost::bind (&App::idle, this));

			if (Config::instance()->check_for_updates ()) {
				UpdateChecker::instance()->run ();
			}
		}
		catch (exception& e)
		{
			if (splash) {
				splash->Destroy ();
			}
			error_dialog (0, _("DCP-o-matic Player could not start."), std_to_wx(e.what()));
		}

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

		wxString config;
		if (parser.Found("c", &config)) {
			Config::override_path = wx_to_std (config);
		}
		wxString stress;
		if (parser.Found("s", &stress)) {
			_stress = wx_to_std (stress);
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
	boost::optional<string> _stress;
};

IMPLEMENT_APP (App)
