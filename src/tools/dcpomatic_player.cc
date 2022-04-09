/*
    Copyright (C) 2017-2021 Carl Hetherington <cth@carlh.net>

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
#include "wx/timer_display.h"
#include "wx/system_information_dialog.h"
#include "wx/player_stress_tester.h"
#include "wx/verify_dcp_progress_dialog.h"
#include "wx/nag_dialog.h"
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
#include "lib/ffmpeg_content.h"
#include "lib/dcpomatic_log.h"
#include "lib/file_log.h"
#include <dcp/cpl.h>
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
#include <boost/bind/bind.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>

#ifdef check
#undef check
#endif

#define MAX_CPLS 32

using std::cout;
using std::dynamic_pointer_cast;
using std::exception;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using std::weak_ptr;
using boost::scoped_array;
using boost::optional;
using boost::thread;
using boost::bind;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
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
		: wxFrame (nullptr, -1, _("DCP-o-matic Player"))
		, _mode (Config::instance()->player_mode())
		, _main_sizer (new wxBoxSizer(wxVERTICAL))
	{
		dcpomatic_log = make_shared<NullLog>();

#if defined(DCPOMATIC_WINDOWS)
		maybe_open_console ();
		cout << "DCP-o-matic Player is starting." << "\n";
#endif

		auto bar = new wxMenuBar;
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

		_viewer = make_shared<FilmViewer>(_overall_panel);
		if (Config::instance()->player_mode() == Config::PLAYER_MODE_DUAL) {
			auto pc = new PlaylistControls (_overall_panel, _viewer);
			_controls = pc;
			pc->ResetFilm.connect (bind(&DOMFrame::reset_film_weak, this, _1));
		} else {
			_controls = new StandardControls (_overall_panel, _viewer, false);
		}
		_viewer->set_dcp_decode_reduction (Config::instance()->decode_reduction ());
		_viewer->set_optimise_for_j2k (true);
		_viewer->PlaybackPermitted.connect (bind(&DOMFrame::playback_permitted, this));
		_viewer->TooManyDropped.connect (bind(&DOMFrame::too_many_frames_dropped, this));
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

		UpdateChecker::instance()->StateChanged.connect (boost::bind(&DOMFrame::update_checker_state_changed, this));
		setup_screen ();

		_stress.LoadDCP.connect (boost::bind(&DOMFrame::load_dcp, this, _1));
	}

	~DOMFrame ()
	{
		/* It's important that this is stopped before our frame starts destroying its children,
		 * otherwise UI elements that it depends on will disappear from under it.
		 */
		_viewer.reset ();
	}

	void setup_main_sizer (Config::PlayerMode mode)
	{
		_main_sizer->Detach (_viewer->panel());
		_main_sizer->Detach (_controls);
		_main_sizer->Detach (_info);
		if (mode != Config::PLAYER_MODE_DUAL) {
			_main_sizer->Add (_viewer->panel(), 1, wxEXPAND);
		}
		_main_sizer->Add (_controls, mode == Config::PLAYER_MODE_DUAL ? 1 : 0, wxEXPAND | wxALL, 6);
		_main_sizer->Add (_info, 0, wxEXPAND | wxALL, 6);
		_overall_panel->SetSizer (_main_sizer);
		_overall_panel->Layout ();
	}

	bool playback_permitted ()
	{
		if (!_film || !Config::instance()->respect_kdm_validity_periods()) {
			return true;
		}

		bool ok = true;
		for (auto i: _film->content()) {
			auto d = dynamic_pointer_cast<DCPContent>(i);
			if (d && !d->kdm_timing_window_valid()) {
				ok = false;
			}
		}

		if (!ok) {
			error_dialog (this, _("The KDM does not allow playback of this content at this time."));
		}

		return ok;
	}


	void too_many_frames_dropped ()
	{
		if (!Config::instance()->nagged(Config::NAG_TOO_MANY_DROPPED_FRAMES)) {
			_viewer->stop ();
		}

		NagDialog::maybe_nag (
			this,
			Config::NAG_TOO_MANY_DROPPED_FRAMES,
			_(wxS("The player is dropping a lot of frames, so playback may not be accurate.\n\n"
			  "<b>This does not necessarily mean that the DCP you are playing is defective!</b>\n\n"
			  "You may be able to improve player performance by:\n"
			  "• choosing 'decode at half resolution' or 'decode at quarter resolution' from the View menu\n"
			  "• using a more powerful computer.\n"
			 ))
			);
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
			// here
			auto dcp = make_shared<DCPContent>(dir);
			auto job = make_shared<ExamineContentJob>(_film, dcp);
			_examine_job_connection = job->Finished.connect(bind(&DOMFrame::add_dcp_to_film, this, weak_ptr<Job>(job), weak_ptr<Content>(dcp)));
			JobManager::instance()->add (job);
			bool const ok = display_progress (_("DCP-o-matic Player"), _("Loading content"));
			if (!ok || !report_errors_from_last_job(this)) {
				return;
			}
			Config::instance()->add_to_player_history (dir);
		} catch (ProjectFolderError &) {
			error_dialog (
				this,
				wxString::Format(_("Could not load a DCP from %s"), std_to_wx(dir.string())),
				_(
					"This looks like a DCP-o-matic project folder, which cannot be loaded into the player.  "
					"Choose the DCP directory inside the DCP-o-matic project folder if that's what you want to play."
				 )
				);
		} catch (dcp::ReadError& e) {
			error_dialog (this, wxString::Format(_("Could not load a DCP from %s"), std_to_wx(dir.string())), std_to_wx(e.what()));
		} catch (DCPError& e) {
			error_dialog (this, wxString::Format(_("Could not load a DCP from %s"), std_to_wx(dir.string())), std_to_wx(e.what()));
		}
	}

	void add_dcp_to_film (weak_ptr<Job> weak_job, weak_ptr<Content> weak_content)
	{
		auto job = weak_job.lock ();
		if (!job || !job->finished_ok()) {
			return;
		}

		auto content = weak_content.lock ();
		if (!content) {
			return;
		}

		_film->add_content (content);
		_stress.set_suspended (false);
	}

	void reset_film_weak (weak_ptr<Film> weak_film)
	{
		auto film = weak_film.lock ();
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
		if (type != ChangeType::DONE || property != Film::Property::CONTENT) {
			return;
		}

		if (_viewer->playing ()) {
			_viewer->stop ();
		}

		/* Start off as Flat */
		_film->set_container (Ratio::from_id("185"));

		for (auto i: _film->content()) {
			auto dcp = dynamic_pointer_cast<DCPContent>(i);

			for (auto j: i->text) {
				j->set_use (true);
			}

			if (i->video) {
				auto const r = Ratio::nearest_from_ratio(i->video->size().ratio());
				if (r->id() == "239") {
					/* Any scope content means we use scope */
					_film->set_container(r);
				}
			}

			/* Any 3D content means we use 3D mode */
			if (i->video && i->video->frame_type() != VideoFrameType::TWO_D) {
				_film->set_three_d (true);
			}
		}

		_viewer->seek (DCPTime(), true);
		_info->triggered_update ();

		set_menu_sensitivity ();

		auto old = _cpl_menu->GetMenuItems();
		for (auto const& i: old) {
			_cpl_menu->Remove (i);
		}

		if (_film->content().size() == 1) {
			/* Offer a CPL menu */
			auto first = dynamic_pointer_cast<DCPContent>(_film->content().front());
			if (first) {
				DCPExaminer ex (first, true);
				int id = ID_view_cpl;
				for (auto i: ex.cpls()) {
					auto j = _cpl_menu->AppendRadioItem(
						id,
						wxString::Format("%s (%s)", std_to_wx(i->annotation_text().get_value_or("")).data(), std_to_wx(i->id()).data())
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

	void examine_content ()
	{
		DCPOMATIC_ASSERT (_film);
		auto dcp = dynamic_pointer_cast<DCPContent>(_film->content().front());
		DCPOMATIC_ASSERT (dcp);
		dcp->examine (_film, shared_ptr<Job>());

		/* Examining content re-creates the TextContent objects, so we must re-enable them */
		for (auto i: dcp->text) {
			i->set_use (true);
		}
	}

	bool report_errors_from_last_job (wxWindow* parent) const
	{
		auto jm = JobManager::instance ();

		DCPOMATIC_ASSERT (!jm->get().empty());

		auto last = jm->get().back();
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
		auto prefs = _file_menu->Append (wxID_PREFERENCES, _("&Preferences...\tCtrl-P"));
#else
		auto edit = new wxMenu;
		auto prefs = edit->Append (wxID_PREFERENCES, _("&Preferences...\tCtrl-P"));
#endif

		prefs->Enable (Config::instance()->have_write_permission());

		_cpl_menu = new wxMenu;

		auto view = new wxMenu;
		auto c = Config::instance()->decode_reduction();
		_view_cpl = view->Append(ID_view_cpl, _("CPL"), _cpl_menu);
		view->AppendSeparator();
		_view_full_screen = view->AppendCheckItem(ID_view_full_screen, _("Full screen\tF11"));
		_view_dual_screen = view->AppendCheckItem(ID_view_dual_screen, _("Dual screen\tShift+F11"));
		setup_menu ();
		view->AppendSeparator();
		view->Append(ID_view_closed_captions, _("Closed captions..."));
		view->AppendSeparator();
		view->AppendRadioItem(ID_view_scale_appropriate, _("Set decode resolution to match display"))->Check(!static_cast<bool>(c));
		view->AppendRadioItem(ID_view_scale_full, _("Decode at full resolution"))->Check(c && c.get() == 0);
		view->AppendRadioItem(ID_view_scale_half, _("Decode at half resolution"))->Check(c && c.get() == 1);
		view->AppendRadioItem(ID_view_scale_quarter, _("Decode at quarter resolution"))->Check(c && c.get() == 2);

		auto tools = new wxMenu;
		_tools_verify = tools->Append (ID_tools_verify, _("Verify DCP..."));
		tools->AppendSeparator ();
		tools->Append (ID_tools_check_for_updates, _("Check for updates"));
		tools->Append (ID_tools_timing, _("Timing..."));
		tools->Append (ID_tools_system_information, _("System information..."));

		auto help = new wxMenu;
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
		auto d = wxStandardPaths::Get().GetDocumentsDir();
		if (Config::instance()->last_player_load_directory()) {
			d = std_to_wx (Config::instance()->last_player_load_directory()->string());
		}

		auto c = new wxDirDialog (this, _("Select DCP to open"), d, wxDEFAULT_DIALOG_STYLE | wxDD_DIR_MUST_EXIST);

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
		auto c = new wxDirDialog (
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
			auto dcp = std::dynamic_pointer_cast<DCPContent>(_film->content().front());
			DCPOMATIC_ASSERT (dcp);
			dcp->add_ov (wx_to_std(c->GetPath()));
			JobManager::instance()->add(make_shared<ExamineContentJob>(_film, dcp));
			bool const ok = display_progress (_("DCP-o-matic Player"), _("Loading content"));
			if (!ok || !report_errors_from_last_job(this)) {
				return;
			}
			for (auto i: dcp->text) {
				i->set_use (true);
			}
			if (dcp->video) {
				auto const r = Ratio::nearest_from_ratio(dcp->video->size().ratio());
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
		auto d = new wxFileDialog (this, _("Select KDM"));

		if (d->ShowModal() == wxID_OK) {
			DCPOMATIC_ASSERT (_film);
			auto dcp = std::dynamic_pointer_cast<DCPContent>(_film->content().front());
			DCPOMATIC_ASSERT (dcp);
			try {
				if (dcp) {
					_viewer->set_coalesce_player_changes (true);
					dcp->add_kdm (dcp::EncryptedKDM(dcp::file_to_string(wx_to_std(d->GetPath()), MAX_KDM_SIZE)));
					examine_content();
					_viewer->set_coalesce_player_changes (false);
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
		auto history = Config::instance()->player_history ();
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
		auto dcp = std::dynamic_pointer_cast<DCPContent>(_film->content().front());
		DCPOMATIC_ASSERT (dcp);
		DCPExaminer ex (dcp, true);
		int id = ev.GetId() - ID_view_cpl;
		DCPOMATIC_ASSERT (id >= 0);
		DCPOMATIC_ASSERT (id < int(ex.cpls().size()));
		auto cpls = ex.cpls();
		auto i = cpls.begin();
		while (id > 0) {
			++i;
			--id;
		}

		_viewer->set_coalesce_player_changes (true);
		dcp->set_cpl ((*i)->id());
		examine_content ();
		_viewer->set_coalesce_player_changes (false);

		_info->triggered_update ();
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
					Move (wxDisplay(0U).GetClientArea().GetWidth(), 0);
					break;
				case 1:
					_dual_screen->Move (wxDisplay(0U).GetClientArea().GetWidth(), 0);
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
		auto dcp = std::dynamic_pointer_cast<DCPContent>(_film->content().front());
		DCPOMATIC_ASSERT (dcp);

		auto job = make_shared<VerifyDCPJob>(dcp->directories());
		auto progress = new VerifyDCPProgressDialog(this, _("DCP-o-matic Player"));
		bool const completed = progress->run (job);
		progress->Destroy ();
		if (!completed) {
			return;
		}

		auto d = new VerifyDCPDialog (this, job);
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
		auto d = new TimerDisplay (this, _viewer->state_timer(), _viewer->gets());
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
		auto d = new AboutDialog (this);
		d->ShowModal ();
		d->Destroy ();
	}

	void help_report_a_problem ()
	{
		auto d = new ReportProblemDialog (this);
		if (d->ShowModal () == wxID_OK) {
			d->report ();
		}
		d->Destroy ();
	}

	void update_checker_state_changed ()
	{
		auto uc = UpdateChecker::instance ();

		bool const announce =
			_update_news_requested ||
			(uc->stable() && Config::instance()->check_for_updates()) ||
			(uc->test() && Config::instance()->check_for_updates() && Config::instance()->check_for_test_updates());

		_update_news_requested = false;

		if (!announce) {
			return;
		}

		if (uc->state() == UpdateChecker::State::YES) {
			auto dialog = new UpdateDialog (this, uc->stable (), uc->test ());
			dialog->ShowModal ();
			dialog->Destroy ();
		} else if (uc->state() == UpdateChecker::State::FAILED) {
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
		_history_separator = nullptr;

		int pos = _history_position;

		/* Clear out non-existant history items before we re-build the menu */
		Config::instance()->clean_player_history ();
		auto history = Config::instance()->player_history ();

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
			auto p = Config::instance()->player_debug_log_file();
			if (p) {
				dcpomatic_log = make_shared<FileLog>(*p);
			} else {
				dcpomatic_log = make_shared<NullLog>();
			}
			dcpomatic_log->set_types (LogEntry::TYPE_GENERAL | LogEntry::TYPE_WARNING | LogEntry::TYPE_ERROR | LogEntry::TYPE_DEBUG_VIDEO_VIEW);
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

	wxFrame* _dual_screen = nullptr;
	bool _update_news_requested = false;
	PlayerInformation* _info = nullptr;
	Config::PlayerMode _mode;
	wxPreferencesEditor* _config_dialog = nullptr;
	wxPanel* _overall_panel = nullptr;
	wxMenu* _file_menu = nullptr;
	wxMenuItem* _view_cpl = nullptr;
	wxMenu* _cpl_menu = nullptr;
	int _history_items = 0;
	int _history_position = 0;
	wxMenuItem* _history_separator = nullptr;
	shared_ptr<FilmViewer> _viewer;
	Controls* _controls;
	SystemInformationDialog* _system_information_dialog = nullptr;
	std::shared_ptr<Film> _film;
	boost::signals2::scoped_connection _config_changed_connection;
	boost::signals2::scoped_connection _examine_job_connection;
	wxMenuItem* _file_add_ov = nullptr;
	wxMenuItem* _file_add_kdm = nullptr;
	wxMenuItem* _tools_verify = nullptr;
	wxMenuItem* _view_full_screen = nullptr;
	wxMenuItem* _view_dual_screen = nullptr;
	wxSizer* _main_sizer = nullptr;
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

	void handle (shared_ptr<Socket> socket) override
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
	{
#ifdef DCPOMATIC_LINUX
		XInitThreads ();
#endif
	}

private:

	bool OnInit () override
	{
		wxSplashScreen* splash = nullptr;
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

#ifdef DCPOMATIC_OSX
			make_foreground_application ();
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
				splash = nullptr;
			}
			_frame->Show ();

			try {
				auto server = new PlayServer (_frame);
				new thread (boost::bind (&PlayServer::run, server));
			} catch (std::exception& e) {
				/* This is not the end of the world; probably a failure to bind the server socket
				 * because there's already another player running.
				 */
				LOG_DEBUG_PLAYER ("Failed to start play server (%1)", e.what());
			}

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

	void OnInitCmdLine (wxCmdLineParser& parser) override
	{
		parser.SetDesc (command_line_description);
		parser.SetSwitchChars (wxT ("-"));
	}

	bool OnCmdLineParsed (wxCmdLineParser& parser) override
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
	bool OnExceptionInMainLoop () override
	{
		report_exception ();
		/* This will terminate the program */
		return false;
	}

	void OnUnhandledException () override
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

	DOMFrame* _frame = nullptr;
	string _dcp_to_load;
	boost::optional<string> _stress;
};

IMPLEMENT_APP (App)
