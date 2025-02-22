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

#include "wx/about_dialog.h"
#include "wx/file_dialog.h"
#include "wx/film_viewer.h"
#include "wx/id.h"
#include "wx/nag_dialog.h"
#include "wx/player_config_dialog.h"
#include "wx/player_information.h"
#include "wx/player_stress_tester.h"
#include "wx/playlist_controls.h"
#include "wx/report_problem_dialog.h"
#include "wx/standard_controls.h"
#include "wx/system_information_dialog.h"
#include "wx/timer_display.h"
#include "wx/update_dialog.h"
#include "wx/verify_dcp_progress_dialog.h"
#include "wx/wx_signal_manager.h"
#include "wx/wx_util.h"
#include "wx/wx_variant.h"
#include "lib/compose.hpp"
#include "lib/config.h"
#include "lib/constants.h"
#include "lib/cross.h"
#include "lib/dcp_content.h"
#include "lib/dcp_examiner.h"
#include "lib/dcpomatic_log.h"
#include "lib/dcpomatic_socket.h"
#include "lib/examine_content_job.h"
#include "lib/ffmpeg_content.h"
#include "lib/file_log.h"
#include "lib/film.h"
#include "lib/font_config.h"
#include "lib/http_server.h"
#include "lib/image.h"
#include "lib/image_jpeg.h"
#include "lib/image_png.h"
#include "lib/internal_player_server.h"
#include "lib/internet.h"
#include "lib/job.h"
#include "lib/job_manager.h"
#include "lib/null_log.h"
#include "lib/player.h"
#include "lib/player_video.h"
#include "lib/ratio.h"
#include "lib/scoped_temporary.h"
#include "lib/server.h"
#include "lib/text_content.h"
#include "lib/update_checker.h"
#include "lib/variant.h"
#include "lib/verify_dcp_job.h"
#include "lib/video_content.h"
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/exceptions.h>
#include <dcp/filesystem.h>
#include <dcp/scope_guard.h>
#include <dcp/search.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/cmdline.h>
#include <wx/display.h>
#include <wx/dnd.h>
#include <wx/preferences.h>
#include <wx/progdlg.h>
#include <wx/splash.h>
#include <wx/stdpaths.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#ifdef __WXGTK__
#include <X11/Xlib.h>
#endif
#include <boost/algorithm/string.hpp>
#include <boost/bind/bind.hpp>
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
using boost::bind;
using boost::optional;
using boost::scoped_array;
using boost::thread;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


enum {
	ID_file_open = DCPOMATIC_MAIN_MENU,
	ID_file_add_ov,
	ID_file_add_kdm,
	ID_file_save_frame,
	ID_file_history,
	/* Allow spare IDs after _history for the recent files list */
	ID_file_close = DCPOMATIC_MAIN_MENU + 100,
	ID_view_cpl,
	/* Allow spare IDs for CPLs */
	ID_view_full_screen = DCPOMATIC_MAIN_MENU + 200,
	ID_view_dual_screen,
	ID_view_closed_captions,
	ID_view_eye,
	ID_view_eye_left,
	ID_view_eye_right,
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
					_frame->load_dcp(wx_to_std(filenames[0]));
					return true;
				}
			}

			if (filenames.GetCount() >= 1) {
				/* Try to load the parent if we drop some files, one if which is an asset map */
				for (size_t i = 0; i < filenames.GetCount(); ++i) {
					auto path = boost::filesystem::path(wx_to_std(filenames[i]));
					if (path.filename() == "ASSETMAP" || path.filename() == "ASSETMAP.xml") {
						_frame->load_dcp(path.parent_path());
						return true;
					}
				}
			}

			return false;
		}

	private:
		DOMFrame* _frame;
	};


	DOMFrame ()
		: wxFrame(nullptr, -1, variant::wx::dcpomatic_player())
		, _mode (Config::instance()->player_mode())
		/* Use a panel as the only child of the Frame so that we avoid
		   the dark-grey background on Windows.
		*/
		, _overall_panel(new wxPanel(this, wxID_ANY))
		, _viewer(_overall_panel)
		, _main_sizer (new wxBoxSizer(wxVERTICAL))
	{
		dcpomatic_log = make_shared<NullLog>();

#if defined(DCPOMATIC_WINDOWS)
		maybe_open_console ();
		cout << variant::dcpomatic_player() << " is starting." << "\n";
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
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_save_frame, this), ID_file_save_frame);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_history, this, _1), ID_file_history, ID_file_history + HISTORY_SIZE);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_close, this), ID_file_close);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::file_exit, this), wxID_EXIT);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::edit_preferences, this), wxID_PREFERENCES);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::view_full_screen, this), ID_view_full_screen);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::view_dual_screen, this), ID_view_dual_screen);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::view_closed_captions, this), ID_view_closed_captions);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::view_cpl, this, _1), ID_view_cpl, ID_view_cpl + MAX_CPLS);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::view_eye_changed, this, _1), ID_view_eye_left);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::view_eye_changed, this, _1), ID_view_eye_right);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::set_decode_reduction, this, optional<int>(0)), ID_view_scale_full);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::set_decode_reduction, this, optional<int>(1)), ID_view_scale_half);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::set_decode_reduction, this, optional<int>(2)), ID_view_scale_quarter);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::help_about, this), wxID_ABOUT);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::help_report_a_problem, this), ID_help_report_a_problem);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_verify, this), ID_tools_verify);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_check_for_updates, this), ID_tools_check_for_updates);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_timing, this), ID_tools_timing);
		Bind (wxEVT_MENU, boost::bind (&DOMFrame::tools_system_information, this), ID_tools_system_information);

		Bind(wxEVT_CLOSE_WINDOW, boost::bind(&DOMFrame::close, this, _1));

		if (Config::instance()->player_mode() == Config::PLAYER_MODE_DUAL) {
			auto pc = new PlaylistControls (_overall_panel, _viewer);
			_controls = pc;
			pc->ResetFilm.connect (bind(&DOMFrame::reset_film_weak, this, _1));
		} else {
			_controls = new StandardControls (_overall_panel, _viewer, false);
		}
		_controls->set_film(_viewer.film());
		_viewer.set_dcp_decode_reduction(Config::instance()->decode_reduction());
		_viewer.PlaybackPermitted.connect(bind(&DOMFrame::playback_permitted, this));
		_viewer.TooManyDropped.connect(bind(&DOMFrame::too_many_frames_dropped, this));
		_info = new PlayerInformation (_overall_panel, _viewer);
		setup_main_sizer (Config::instance()->player_mode());
#ifdef __WXOSX__
		int accelerators = 12;
#else
		int accelerators = 11;
#endif

		_stress.setup (this, _controls);

		std::vector<wxAcceleratorEntry> accel(accelerators);
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
		wxAcceleratorTable accel_table (accelerators, accel.data());
		SetAcceleratorTable (accel_table);

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

		setup_internal_player_server();
		setup_http_server();

		SetDropTarget(new DCPDropTarget(this));
	}

	~DOMFrame ()
	{
		stop_http_server();
		/* It's important that this is stopped before our frame starts destroying its children,
		 * otherwise UI elements that it depends on will disappear from under it.
		 */
		_viewer.stop();
	}

	void close(wxCloseEvent& ev)
	{
		FontConfig::drop();
		ev.Skip();
	}

	void setup_main_sizer (Config::PlayerMode mode)
	{
		_main_sizer->Detach(_viewer.panel());
		_main_sizer->Detach (_controls);
		_main_sizer->Detach (_info);
		if (mode != Config::PLAYER_MODE_DUAL) {
			_main_sizer->Add(_viewer.panel(), 1, wxEXPAND);
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
			_viewer.stop();
		}

		NagDialog::maybe_nag (
			this,
			Config::NAG_TOO_MANY_DROPPED_FRAMES,
			wxGetTranslation(
				wxString::FromUTF8(
					"The player is dropping a lot of frames, so playback may not be accurate.\n\n"
				        "<b>This does not necessarily mean that the DCP you are playing is defective!</b>\n\n"
				        "You may be able to improve player performance by:\n"
				        "• choosing 'decode at half resolution' or 'decode at quarter resolution' from the View menu\n"
				        "• using a more powerful computer.\n"
					)
				)
			);
	}

	void set_decode_reduction (optional<int> reduction)
	{
		_viewer.set_dcp_decode_reduction(reduction);
		_info->triggered_update ();
		Config::instance()->set_decode_reduction (reduction);
	}

	void load_dcp (boost::filesystem::path dir)
	{
		DCPOMATIC_ASSERT (_film);

		auto film = std::make_shared<Film>(optional<boost::filesystem::path>());

		try {
			_stress.set_suspended (true);

			/* Handler to set things up once the DCP has been examined */
			auto setup = [this](weak_ptr<Film> weak_film, weak_ptr<Job> weak_job, weak_ptr<Content> weak_content)
			{
				auto job = weak_job.lock();
				if (!job || !job->finished_ok()) {
					return;
				}

				auto content = weak_content.lock();
				if (!content) {
					return;
				}

				auto film = weak_film.lock();
				if (!film) {
					return;
				}

				film->add_content(content);
				_stress.set_suspended(false);
				reset_film(film);
			};

			auto dcp = make_shared<DCPContent>(dir);
			auto job = make_shared<ExamineContentJob>(film, dcp, true);
			_examine_job_connection = job->Finished.connect(boost::bind<void>(setup, weak_ptr<Film>(film), weak_ptr<Job>(job), weak_ptr<Content>(dcp)));
			JobManager::instance()->add (job);
			bool const ok = display_progress(variant::wx::dcpomatic_player(), _("Loading content"));
			if (ok && report_errors_from_last_job(this)) {
				Config::instance()->add_to_player_history(dir);
			}
		} catch (ProjectFolderError &) {
			error_dialog (
				this,
				wxString::Format(_("Could not load a DCP from %s"), std_to_wx(dir.string())),
				wxString::Format(
					_("This looks like a %s project folder, which cannot be loaded into the player.  "
					  "Choose the DCP folder inside the %s project folder if that's what you want to play."),
					variant::wx::dcpomatic(),
					variant::wx::dcpomatic()
					)
				);
		} catch (dcp::ReadError& e) {
			error_dialog (this, wxString::Format(_("Could not load a DCP from %s"), std_to_wx(dir.string())), std_to_wx(e.what()));
		} catch (DCPError& e) {
			error_dialog (this, wxString::Format(_("Could not load a DCP from %s"), std_to_wx(dir.string())), std_to_wx(e.what()));
		}
	}

	void reset_film_weak (weak_ptr<Film> weak_film)
	{
		auto film = weak_film.lock ();
		if (film) {
			reset_film (film);
		}
	}

	void reset_film(shared_ptr<Film> film = std::make_shared<Film>(boost::none))
	{
		_film = film;
		film_changed();

		_viewer.set_film(_film);
		_viewer.seek(DCPTime(), true);
		_viewer.set_eyes(_view_eye_left->IsChecked() ? Eyes::LEFT : Eyes::RIGHT);
		_info->triggered_update();
		set_menu_sensitivity();

		_controls->set_film (_film);
	}

	/* Update anything that depends on properties of the film or its contents */
	void film_changed()
	{
		if (_viewer.playing()) {
			_viewer.stop();
		}

		/* Start off as Flat */
		_film->set_container (Ratio::from_id("185"));

		_film->set_audio_channels (MAX_DCP_AUDIO_CHANNELS);

		for (auto i: _film->content()) {
			auto dcp = dynamic_pointer_cast<DCPContent>(i);

			for (auto j: i->text) {
				j->set_use (true);
			}

			if (i->video && i->video->size()) {
				auto const r = Ratio::nearest_from_ratio(i->video->size()->ratio());
				if (r->id() == "239") {
					/* Any scope content means we use scope */
					_film->set_container(r);
				}
			}

			/* Any 3D content means we use 3D mode */
			if (i->video && i->video->frame_type() != VideoFrameType::TWO_D) {
				_film->set_three_d (true);
			}

			if (dcp->video_frame_rate()) {
				_film->set_video_frame_rate(dcp->video_frame_rate().get(), true);
			}

			switch (dcp->video_encoding()) {
			case VideoEncoding::JPEG2000:
				_viewer.set_optimisation(Optimisation::JPEG2000);
				break;
			case VideoEncoding::MPEG2:
				_viewer.set_optimisation(Optimisation::MPEG2);
				break;
			case VideoEncoding::COUNT:
				DCPOMATIC_ASSERT(false);
			}
		}

		auto old = _cpl_menu->GetMenuItems();
		for (auto const& i: old) {
			_cpl_menu->Remove (i);
		}

		if (_film->content().size() == 1) {
			/* Offer a CPL menu */
			auto first = dynamic_pointer_cast<DCPContent>(_film->content().front());
			if (first) {
				int id = ID_view_cpl;
				for (auto i: dcp::find_and_resolve_cpls(first->directories(), true)) {
					auto j = _cpl_menu->AppendRadioItem(
						id,
						wxString::Format(char_to_wx("%s (%s)"), std_to_wx(i->content_title_text()).data(), std_to_wx(i->id()).data())
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

	void idle()
	{
		if (_http_server) {
			struct timeval now;
			gettimeofday(&now, 0);
			auto time_since_last_update = (now.tv_sec + now.tv_usec / 1e6) - (_last_http_server_update.tv_sec + _last_http_server_update.tv_usec / 1e6);
			if (time_since_last_update > 0.25) {
				_http_server->set_playing(_viewer.playing());
				if (auto dcp = _viewer.dcp()) {
					_http_server->set_dcp_name(dcp->name());
				} else {
					_http_server->set_dcp_name("");
				}
				_http_server->set_position(_viewer.position());
				_last_http_server_update = now;
			}
		}
	}

private:

	void examine_content ()
	{
		DCPOMATIC_ASSERT (_film);
		auto dcp = dynamic_pointer_cast<DCPContent>(_film->content().front());
		DCPOMATIC_ASSERT (dcp);
		dcp->examine(_film, {}, true);

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
		_file_menu->AppendSeparator ();
		_file_save_frame = _file_menu->Append (ID_file_save_frame, _("&Save frame to file...\tCtrl-S"));

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
		auto prefs = _file_menu->Append(wxID_PREFERENCES, _("&Preferences...\tCtrl-,"));
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
		_view_eye_menu = new wxMenu;
		_view_eye_left = _view_eye_menu->AppendRadioItem(ID_view_eye_left, _("Left"));
		_view_eye_menu->AppendRadioItem(ID_view_eye_right, _("Right"));
		_view_eye = view->Append(ID_view_eye, _("Eye"), _view_eye_menu);
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
		help->Append(wxID_ABOUT, variant::wx::insert_dcpomatic_player(_("About %s")));
#else
		help->Append (wxID_ABOUT, _("About"));
#endif
		if (variant::show_report_a_problem()) {
			help->Append(ID_help_report_a_problem, _("Report a problem..."));
		}

		m->Append (_file_menu, _("&File"));
		if (!Config::instance()->player_restricted_menus()) {
#ifndef __WXOSX__
			m->Append (edit, _("&Edit"));
#endif
			m->Append (view, _("&View"));
			m->Append (tools, _("&Tools"));
			m->Append (help, _("&Help"));
		}
	}

	void file_open ()
	{
		auto d = wxStandardPaths::Get().GetDocumentsDir();
		if (Config::instance()->last_player_load_directory()) {
			d = std_to_wx (Config::instance()->last_player_load_directory()->string());
		}

		wxDirDialog dialog(this, _("Select DCP to open"), d, wxDEFAULT_DIALOG_STYLE | wxDD_DIR_MUST_EXIST);

		int r;
		while (true) {
			r = dialog.ShowModal();
			if (r == wxID_OK && dialog.GetPath() == wxStandardPaths::Get().GetDocumentsDir()) {
				error_dialog (this, _("You did not select a folder.  Make sure that you select a folder before clicking Open."));
			} else {
				break;
			}
		}

		if (r == wxID_OK) {
			boost::filesystem::path const dcp(wx_to_std(dialog.GetPath ()));
			load_dcp (dcp);
			Config::instance()->set_last_player_load_directory (dcp.parent_path());
		}
	}

	void file_add_ov ()
	{
		auto initial_dir = wxStandardPaths::Get().GetDocumentsDir();
		if (Config::instance()->last_player_load_directory()) {
			initial_dir = std_to_wx(Config::instance()->last_player_load_directory()->string());
		}

		wxDirDialog dialog(
			this,
			_("Select DCP to open as OV"),
			initial_dir,
			wxDEFAULT_DIALOG_STYLE | wxDD_DIR_MUST_EXIST
			);

		int r;
		while (true) {
			r = dialog.ShowModal();
			if (r == wxID_OK && dialog.GetPath() == wxStandardPaths::Get().GetDocumentsDir()) {
				error_dialog (this, _("You did not select a folder.  Make sure that you select a folder before clicking Open."));
			} else {
				break;
			}
		}

		if (r == wxID_OK) {
			DCPOMATIC_ASSERT(_film);
			DCPOMATIC_ASSERT(!_film->content().empty());
			auto dcp = std::dynamic_pointer_cast<DCPContent>(_film->content().front());
			DCPOMATIC_ASSERT(dcp);

			try {
				dcp->add_ov(wx_to_std(dialog.GetPath()));
			} catch (DCPError& e) {
				error_dialog(this, char_to_wx(e.what()));
				return;
			}

			auto job = make_shared<ExamineContentJob>(_film, dcp, true);

			auto film_ready = [this]() {
				film_changed();
				_viewer.set_film(_film);
				_viewer.seek(DCPTime(), true);
				_info->triggered_update();
				set_menu_sensitivity();
			};

			_examine_job_connection = job->Finished.connect(boost::bind<void>(film_ready));
			JobManager::instance()->add(job);

			display_progress(variant::wx::dcpomatic_player(), _("Loading content"));
			report_errors_from_last_job(this);
		}
	}

	void file_add_kdm ()
	{
		FileDialog dialog(this, _("Select KDM"), char_to_wx("XML files|*.xml|All files|*.*"), wxFD_MULTIPLE, "AddKDMPath");

		if (dialog.show()) {
			DCPOMATIC_ASSERT (_film);
			auto dcp = std::dynamic_pointer_cast<DCPContent>(_film->content().front());
			DCPOMATIC_ASSERT (dcp);
			try {
				if (dcp) {
					dcp::ScopeGuard sg([this]() {
						_viewer.set_coalesce_player_changes(false);
					});
					_viewer.set_coalesce_player_changes(true);
					for (auto path: dialog.paths()) {
						dcp->add_kdm(dcp::EncryptedKDM(dcp::file_to_string(path)));
						_kdms.push_back(path);
					}
					examine_content();
				}
			} catch (exception& e) {
				error_dialog (this, wxString::Format (_("Could not load KDM.")), std_to_wx(e.what()));
				return;
			}
		}

		_info->triggered_update ();
	}

	void file_save_frame ()
	{
		wxFileDialog dialog (this, _("Save frame to file"), {}, {}, char_to_wx("PNG files (*.png)|*.png|JPEG files (*.jpg;*.jpeg)|*.jpg;*.jpeg"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
		if (dialog.ShowModal() == wxID_CANCEL) {
			return;
		}

		auto path = boost::filesystem::path (wx_to_std(dialog.GetPath()));

		auto player = make_shared<Player>(_film, Image::Alignment::PADDED, true);
		player->seek(_viewer.position(), true);

		bool done = false;
		player->Video.connect ([path, &done, this](shared_ptr<PlayerVideo> video, DCPTime) {
			auto ext = boost::algorithm::to_lower_copy(path.extension().string());
			if (ext == ".png") {
				auto image = video->image(boost::bind(PlayerVideo::force, AV_PIX_FMT_RGBA), VideoRange::FULL, false);
				image_as_png(image).write(path);
			} else if (ext == ".jpg" || ext == ".jpeg") {
				auto image = video->image(boost::bind(PlayerVideo::force, AV_PIX_FMT_RGB24), VideoRange::FULL, false);
				image_as_jpeg(image, 80).write(path);
			} else {
				error_dialog(this, wxString::Format(_("Unrecognised file extension %s (use .jpg, .jpeg or .png)"), std_to_wx(ext)));
			}
			done = true;
		});

		int tries_left = 50;
		while (!done && tries_left >= 0) {
			player->pass();
			--tries_left;
		}

		DCPOMATIC_ASSERT (tries_left >= 0);
	}

	void file_history (wxCommandEvent& event)
	{
		auto history = Config::instance()->player_history ();
		int n = event.GetId() - ID_file_history;
		if (n >= 0 && n < static_cast<int> (history.size ())) {
			try {
				load_dcp (history[n]);
			} catch (exception& e) {
				error_dialog(nullptr, wxString::Format(_("Could not load DCP %s."), std_to_wx(history[n].string()))), std_to_wx(e.what());
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
		auto cpls = dcp::find_and_resolve_cpls (dcp->directories(), true);
		int id = ev.GetId() - ID_view_cpl;
		DCPOMATIC_ASSERT (id >= 0);
		DCPOMATIC_ASSERT (id < int(cpls.size()));
		auto i = cpls.begin();
		while (id > 0) {
			++i;
			--id;
		}

		_viewer.set_coalesce_player_changes(true);
		dcp->set_cpl ((*i)->id());
		examine_content ();
		_viewer.set_coalesce_player_changes(false);

		_info->triggered_update ();
	}

	void view_eye_changed(wxCommandEvent& ev)
	{
		_viewer.set_eyes(ev.GetId() == ID_view_eye_left ? Eyes::LEFT : Eyes::RIGHT);
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
		_viewer.set_pad_black(_mode != Config::PLAYER_MODE_WINDOW);

		if (_mode == Config::PLAYER_MODE_DUAL) {
			_dual_screen = new wxFrame(this, wxID_ANY, {});
			_dual_screen->SetBackgroundColour (wxColour(0, 0, 0));
			_dual_screen->ShowFullScreen (true);
			_viewer.panel()->Reparent(_dual_screen);
			_viewer.panel()->SetFocus();
			_dual_screen->Show ();
			LOG_DEBUG_PLAYER("Setting up dual screen mode with %1 displays", wxDisplay::GetCount());
			for (auto index = 0U; index < wxDisplay::GetCount(); ++index) {
				wxDisplay display(index);
				auto client = display.GetClientArea();
				auto mode = display.GetCurrentMode();
				auto geometry = display.GetGeometry();
				LOG_DEBUG_PLAYER("Display %1", index);
				LOG_DEBUG_PLAYER("  ClientArea position=(%1, %2) size=(%3, %4)", client.GetX(), client.GetY(), client.GetWidth(), client.GetHeight());
				LOG_DEBUG_PLAYER("  Geometry   position=(%1, %2) size=(%3, %4)", geometry.GetX(), geometry.GetY(), geometry.GetWidth(), geometry.GetHeight());
				LOG_DEBUG_PLAYER("  Mode       size=(%1, %2)", mode.GetWidth(), mode.GetHeight());
				LOG_DEBUG_PLAYER("  Primary?   %1", static_cast<int>(display.IsPrimary()));
			}
			if (wxDisplay::GetCount() > 1) {
				wxRect geometry[2] = {
					wxDisplay(0U).GetGeometry(),
					wxDisplay(1U).GetGeometry()
				};
				auto const image_display = Config::instance()->image_display();
				_dual_screen->Move(geometry[image_display].GetX(), geometry[image_display].GetY());
				_viewer.panel()->SetSize(geometry[image_display].GetWidth(), geometry[image_display].GetHeight());
				Move(geometry[1 - image_display].GetX(), geometry[1 - image_display].GetY());
			}
			_dual_screen->Bind(wxEVT_CHAR_HOOK, boost::bind(&DOMFrame::dual_screen_key_press, this, _1));
		} else {
			if (_dual_screen) {
				_viewer.panel()->Reparent(_overall_panel);
				_dual_screen->Destroy ();
				_dual_screen = 0;
			}
		}

		setup_main_sizer (_mode);
	}

	void dual_screen_key_press(wxKeyEvent& ev)
	{
		if (ev.GetKeyCode() == WXK_F11) {
			if (ev.ShiftDown()) {
				view_dual_screen();
			} else if (!ev.HasAnyModifiers()) {
				view_full_screen();
			}
		}
	}

	void view_closed_captions ()
	{
		_viewer.show_closed_captions();
	}

	void tools_verify ()
	{
		DCPOMATIC_ASSERT(!_film->content().empty());
		auto dcp = std::dynamic_pointer_cast<DCPContent>(_film->content().front());
		DCPOMATIC_ASSERT (dcp);

		auto job = make_shared<VerifyDCPJob>(dcp->directories(), _kdms, dcp::VerificationOptions{});
		VerifyDCPProgressDialog progress(this, _("Verify DCP"), job);
		progress.ShowModal();
	}

	void tools_check_for_updates ()
	{
		UpdateChecker::instance()->run ();
		_update_news_requested = true;
	}

	void tools_timing ()
	{
		TimerDisplay dialog(this, _viewer.state_timer(), _viewer.gets());
		dialog.ShowModal();
	}

	void tools_system_information ()
	{
		if (!_system_information_dialog) {
			_system_information_dialog.emplace(this, _viewer);
		}

		_system_information_dialog->Show ();
	}

	void help_about ()
	{
		AboutDialog dialog(this);
		dialog.ShowModal();
	}

	void help_report_a_problem ()
	{
		ReportProblemDialog dialog(this);
		if (dialog.ShowModal() == wxID_OK) {
			dialog.report();
		}
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
			UpdateDialog dialog(this, uc->stable (), uc->test ());
			dialog.ShowModal();
		} else if (uc->state() == UpdateChecker::State::FAILED) {
			error_dialog(this, variant::wx::insert_dcpomatic(_("The %s download server could not be contacted.")));
		} else {
			error_dialog(this, variant::wx::insert_dcpomatic(_("There are no new versions of %s available.")));
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

		setup_http_server();
	}

	void stop_http_server()
	{
		if (_http_server) {
			_http_server->stop();
			_http_server_thread.join();
			_http_server.reset();
		}
	}

	void setup_http_server()
	{
		stop_http_server();

		auto config = Config::instance();
		try {
			if (config->enable_player_http_server()) {
				_http_server.reset(new HTTPServer(config->player_http_server_port()));
				_http_server->Play.connect(boost::bind(&FilmViewer::start, &_viewer));
				_http_server->Stop.connect(boost::bind(&FilmViewer::stop, &_viewer));
				_http_server_thread = boost::thread(boost::bind(&HTTPServer::run, _http_server.get()));
			}
		} catch (std::exception& e) {
			LOG_DEBUG_PLAYER("Failed to start player HTTP server (%1)", e.what());
		}
	}

	void setup_internal_player_server()
	{
		try {
			auto server = new InternalPlayerServer();
			server->LoadDCP.connect(boost::bind(&DOMFrame::load_dcp, this, _1));
			new thread(boost::bind(&InternalPlayerServer::run, server));
		} catch (std::exception& e) {
			/* This is not the end of the world; probably a failure to bind the server socket
			 * because there's already another player running.
			 */
			LOG_DEBUG_PLAYER("Failed to start internal player server (%1)", e.what());
		}
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

		/* Clear out non-existent history items before we re-build the menu */
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
		}
	}

	void set_menu_sensitivity ()
	{
		auto const enable = _film && !_film->content().empty();
		_tools_verify->Enable(enable);
		_file_add_ov->Enable(enable);
		_file_add_kdm->Enable(enable);
		_file_save_frame->Enable(enable);
		_view_cpl->Enable(enable);
		_view_eye->Enable(enable && _film->three_d());
	}

	void start_stop_pressed ()
	{
		if (_viewer.playing()) {
			_viewer.stop();
		} else {
			_viewer.start();
		}
	}

	void go_back_frame ()
	{
		_viewer.seek_by(-_viewer.one_video_frame(), true);
	}

	void go_forward_frame ()
	{
		_viewer.seek_by(_viewer.one_video_frame(), true);
	}

	void go_seconds (int s)
	{
		_viewer.seek_by(DCPTime::from_seconds(s), true);
	}

	void go_to_start ()
	{
		_viewer.seek(DCPTime(), true);
	}

	void go_to_end ()
	{
		_viewer.seek(_film->length() - _viewer.one_video_frame(), true);
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
	wxMenuItem* _view_eye = nullptr;
	wxMenuItem* _view_eye_left = nullptr;
	wxMenu* _view_eye_menu = nullptr;
	int _history_items = 0;
	int _history_position = 0;
	wxMenuItem* _history_separator = nullptr;
	FilmViewer _viewer;
	Controls* _controls;
	boost::optional<SystemInformationDialog> _system_information_dialog;
	std::shared_ptr<Film> _film;
	boost::signals2::scoped_connection _config_changed_connection;
	boost::signals2::scoped_connection _examine_job_connection;
	wxMenuItem* _file_add_ov = nullptr;
	wxMenuItem* _file_add_kdm = nullptr;
	wxMenuItem* _file_save_frame = nullptr;
	wxMenuItem* _tools_verify = nullptr;
	wxMenuItem* _view_full_screen = nullptr;
	wxMenuItem* _view_dual_screen = nullptr;
	wxSizer* _main_sizer = nullptr;
	PlayerStressTester _stress;
	/** KDMs that have been loaded, so that we can pass them to the verifier */
	std::vector<boost::filesystem::path> _kdms;
	boost::thread _http_server_thread;
	std::unique_ptr<HTTPServer> _http_server;
	struct timeval _last_http_server_update = { 0, 0 };
};

static const wxCmdLineEntryDesc command_line_description[] = {
	{ wxCMD_LINE_PARAM, 0, 0, "DCP to load or create", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_OPTION, "c", "config", "Directory containing config.xml", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
	{ wxCMD_LINE_OPTION, "s", "stress", "File containing description of stress test", wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
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
	{
#ifdef DCPOMATIC_LINUX
		XInitThreads ();
#endif
	}

private:

	bool OnInit () override
	{
		wxSplashScreen* splash;
		try {
			wxInitAllImageHandlers ();

			Config::FailedToLoad.connect (boost::bind (&App::config_failed_to_load, this));
			Config::Warning.connect (boost::bind (&App::config_warning, this, _1));

			splash = maybe_show_splash ();

			SetAppName(variant::wx::dcpomatic_player());

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

			if (!_dcp_to_load.empty() && dcp::filesystem::is_directory(_dcp_to_load)) {
				try {
					_frame->load_dcp (_dcp_to_load);
				} catch (exception& e) {
					error_dialog(nullptr, wxString::Format(_("Could not load DCP %s"), std_to_wx(_dcp_to_load)), std_to_wx(e.what()));
				}
			}

			if (_stress) {
				try {
					_frame->load_stress_script (*_stress);
				} catch (exception& e) {
					error_dialog(nullptr, wxString::Format(_("Could not load stress test file %s"), std_to_wx(*_stress)));
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
			error_dialog(nullptr, variant::wx::insert_dcpomatic_player(_("%s could not start")), std_to_wx(e.what()));
		}

		return true;
	}

	void OnInitCmdLine (wxCmdLineParser& parser) override
	{
		parser.SetDesc (command_line_description);
		parser.SetSwitchChars(char_to_wx("-"));
	}

	bool OnCmdLineParsed (wxCmdLineParser& parser) override
	{
		if (parser.GetParamCount() > 0) {
			_dcp_to_load = wx_to_std (parser.GetParam (0));
		}

		wxString config;
		if (parser.Found(char_to_wx("c"), &config)) {
			Config::override_path = wx_to_std (config);
		}
		wxString stress;
		if (parser.Found(char_to_wx("s"), &stress)) {
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
					_("An exception occurred: %s (%s)\n\n%s"),
					std_to_wx(e.what()),
					std_to_wx(e.file().string().c_str()),
					wx::report_problem()
					)
				);
		} catch (exception& e) {
			error_dialog (
				0,
				wxString::Format (
					_("An exception occurred: %s\n\n%s"),
					std_to_wx(e.what()),
					wx::report_problem()
					)
				);
		} catch (...) {
			error_dialog(nullptr, wxString::Format(_("An unknown exception occurred. %s"), wx::report_problem()));
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
		if (_frame) {
			_frame->idle();
		}
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
