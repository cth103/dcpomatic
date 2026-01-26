/*
    Copyright (C) 2017-2026 Carl Hetherington <cth@carlh.net>

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


#include "about_dialog.h"
#include "id.h"
#include "file_dialog.h"
#include "nag_dialog.h"
#include "standard_controls.h"
#include "player_config_dialog.h"
#include "player_information.h"
#include "player_frame.h"
#include "playlist_controls.h"
#include "report_problem_dialog.h"
#include "timer_display.h"
#include "update_dialog.h"
#include "verify_dcp_dialog.h"
#include "wx_util.h"
#include "wx_variant.h"
#include "lib/audio_content.h"
#include "lib/copy_dcp_details_to_film.h"
#include "lib/dcp_content.h"
#include "lib/dcpomatic_log.h"
#include "lib/examine_content_job.h"
#include "lib/image.h"
#include "lib/image_jpeg.h"
#include "lib/image_png.h"
#include "lib/internal_player_server.h"
#include "lib/file_log.h"
#include "lib/film.h"
#include "lib/font_config.h"
#include "lib/job_manager.h"
#include "lib/null_log.h"
#include "lib/show_playlist_content_store.h"
#include "lib/text_content.h"
#include "lib/update_checker.h"
#include "lib/variant.h"
#include "lib/video_content.h"
#include <dcp/cpl.h>
#include <dcp/scope_guard.h>
#include <dcp/search.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/display.h>
#include <wx/preferences.h>
#include <wx/progdlg.h>
#include <wx/stdpaths.h>
LIBDCP_ENABLE_WARNINGS


#define MAX_CPLS 32


using std::dynamic_pointer_cast;
using std::exception;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::weak_ptr;
using std::vector;
using boost::bind;
using boost::optional;
using boost::thread;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic::ui;


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
	ID_tools_audio_graph,
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


PlayerFrame::DCPDropTarget::DCPDropTarget(PlayerFrame* owner)
	: _frame(owner)
{

}


bool
PlayerFrame::DCPDropTarget::OnDropFiles(wxCoord, wxCoord, wxArrayString const& filenames)
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



PlayerFrame::PlayerFrame()
	: wxFrame(nullptr, -1, variant::wx::dcpomatic_player())
	, _mode(Config::instance()->player_mode())
	/* Use a panel as the only child of the Frame so that we avoid
	   the dark-grey background on Windows.
	*/
	, _overall_panel(new wxPanel(this, wxID_ANY))
	, _viewer(_overall_panel, true)
	, _main_sizer(new wxBoxSizer(wxVERTICAL))
{
	dcpomatic_log = make_shared<NullLog>();

#if defined(DCPOMATIC_WINDOWS)
	maybe_open_console();
	cout << variant::dcpomatic_player() << " is starting." << "\n";
#endif

	auto bar = new wxMenuBar;
	setup_menu(bar);
	set_menu_sensitivity();
	SetMenuBar(bar);

#ifdef DCPOMATIC_WINDOWS
	SetIcon(wxIcon(std_to_wx("id")));
#endif

	_config_changed_connection = Config::instance()->Changed.connect(boost::bind(&PlayerFrame::config_changed, this, _1));
	update_from_config(Config::PLAYER_DEBUG_LOG);

	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::file_open, this), ID_file_open);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::file_add_ov, this), ID_file_add_ov);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::file_add_kdm, this), ID_file_add_kdm);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::file_save_frame, this), ID_file_save_frame);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::file_history, this, _1), ID_file_history, ID_file_history + HISTORY_SIZE);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::file_close, this), ID_file_close);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::file_exit, this), wxID_EXIT);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::edit_preferences, this), wxID_PREFERENCES);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::view_full_screen, this), ID_view_full_screen);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::view_dual_screen, this), ID_view_dual_screen);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::view_closed_captions, this), ID_view_closed_captions);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::view_cpl, this, _1), ID_view_cpl, ID_view_cpl + MAX_CPLS);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::view_eye_changed, this, _1), ID_view_eye_left);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::view_eye_changed, this, _1), ID_view_eye_right);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::set_decode_reduction, this, optional<int>(0)), ID_view_scale_full);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::set_decode_reduction, this, optional<int>(1)), ID_view_scale_half);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::set_decode_reduction, this, optional<int>(2)), ID_view_scale_quarter);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::help_about, this), wxID_ABOUT);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::help_report_a_problem, this), ID_help_report_a_problem);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::tools_verify, this), ID_tools_verify);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::tools_audio_graph, this), ID_tools_audio_graph);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::tools_check_for_updates, this), ID_tools_check_for_updates);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::tools_timing, this), ID_tools_timing);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::tools_system_information, this), ID_tools_system_information);

	Bind(wxEVT_CLOSE_WINDOW, boost::bind(&PlayerFrame::close, this, _1));

	update_content_store();

	if (Config::instance()->player_mode() == Config::PlayerMode::DUAL) {
		_controls = new PlaylistControls(_overall_panel, this, _viewer);
	} else {
		_controls = new StandardControls(_overall_panel, _viewer, false);
	}
	_controls->set_film(_viewer.film());
	_viewer.set_dcp_decode_reduction(Config::instance()->decode_reduction());
	_viewer.PlaybackPermitted.connect(bind(&PlayerFrame::playback_permitted, this));
	_viewer.TooManyDropped.connect(bind(&PlayerFrame::too_many_frames_dropped, this));
	_viewer.Finished.connect(boost::bind(&PlayerFrame::viewer_finished, this));
	_info = new PlayerInformation(_overall_panel, _viewer);
	setup_main_sizer(Config::instance()->player_mode());
#ifdef __WXOSX__
	int accelerators = 12;
#else
	int accelerators = 11;
#endif

	_stress.setup(this, _controls);

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
	wxAcceleratorTable accel_table(accelerators, accel.data());
	SetAcceleratorTable(accel_table);

	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::start_stop_pressed, this), ID_start_stop);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::go_back_frame, this),      ID_go_back_frame);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::go_forward_frame, this),   ID_go_forward_frame);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::go_seconds,  this,   -60), ID_go_back_small_amount);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::go_seconds,  this,    60), ID_go_forward_small_amount);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::go_seconds,  this,  -600), ID_go_back_medium_amount);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::go_seconds,  this,   600), ID_go_forward_medium_amount);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::go_seconds,  this, -3600), ID_go_back_large_amount);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::go_seconds,  this,  3600), ID_go_forward_large_amount);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::go_to_start, this), ID_go_to_start);
	Bind(wxEVT_MENU, boost::bind(&PlayerFrame::go_to_end,   this), ID_go_to_end);

	take_playlist_entry();

	UpdateChecker::instance()->StateChanged.connect(boost::bind(&PlayerFrame::update_checker_state_changed, this));
	setup_screen();

	_stress.LoadDCP.connect(boost::bind(&PlayerFrame::load_dcp, this, _1));

	setup_internal_player_server();
	setup_http_server();

	SetDropTarget(new DCPDropTarget(this));
}


PlayerFrame::~PlayerFrame()
{
	try {
		stop_http_server();
		/* It's important that this is stopped before our frame starts destroying its children,
		 * otherwise UI elements that it depends on will disappear from under it.
		 */
		_viewer.stop();
	} catch (std::exception& e) {
		LOG_ERROR("Destructor threw {}", e.what());
	} catch (...) {
		LOG_ERROR("Destructor threw");
	}
}


void
PlayerFrame::close(wxCloseEvent& ev)
{
	FontConfig::drop();
	ev.Skip();
}


void
PlayerFrame::setup_main_sizer(Config::PlayerMode mode)
{
	_main_sizer->Detach(_viewer.panel());
	_main_sizer->Detach(_controls);
	_main_sizer->Detach(_info);
	if (mode != Config::PlayerMode::DUAL) {
		_main_sizer->Add(_viewer.panel(), 1, wxEXPAND);
	}
	_main_sizer->Add(_controls, mode == Config::PlayerMode::DUAL ? 1 : 0, wxEXPAND | wxALL, 6);
	_main_sizer->Add(_info, 0, wxEXPAND | wxALL, 6);
	_overall_panel->SetSizer(_main_sizer);
	_overall_panel->Layout();
}


bool
PlayerFrame::playback_permitted()
{
	if (!Config::instance()->respect_kdm_validity_periods()) {
		return true;
	}

	bool ok = true;
	for (auto content: _playlist) {
		auto dcp = dynamic_pointer_cast<DCPContent>(content.first);
		if (dcp && !dcp->kdm_timing_window_valid()) {
			ok = false;
		}
	}

	if (!ok) {
		error_dialog(this, _("The KDM does not allow playback of this content at this time."));
	}

	return ok;
}


void
PlayerFrame::too_many_frames_dropped()
{
	if (!Config::instance()->nagged(Config::NAG_TOO_MANY_DROPPED_FRAMES)) {
		_viewer.stop();
	}

	NagDialog::maybe_nag(
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


void
PlayerFrame::set_decode_reduction(optional<int> reduction)
{
	_viewer.set_dcp_decode_reduction(reduction);
	_info->triggered_update();
	Config::instance()->set_decode_reduction(reduction);
}


void
PlayerFrame::load_dcp(boost::filesystem::path dir)
{
	try {
		_stress.set_suspended(true);

		/* Handler to set things up once the DCP has been examined */
		auto setup = [this](weak_ptr<Job> weak_job, weak_ptr<Content> weak_content)
		{
			auto job = weak_job.lock();
			if (!job || !job->finished_ok()) {
				return;
			}

			if (auto content = weak_content.lock()) {
				_playlist = { make_pair(content, boost::optional<float>()) };
				_playlist_position = 0;
				_controls->playlist_changed();
				take_playlist_entry();
			}

			_stress.set_suspended(false);
		};

		auto dcp = make_shared<DCPContent>(dir);
		auto job = make_shared<ExamineContentJob>(vector<shared_ptr<Content>>{dcp}, true);
		_examine_job_connection = job->Finished.connect(boost::bind<void>(setup, weak_ptr<Job>(job), weak_ptr<Content>(dcp)));
		JobManager::instance()->add(job);
		bool const ok = display_progress(variant::wx::dcpomatic_player(), _("Loading content"));
		if (ok && report_errors_from_last_job(this)) {
			Config::instance()->add_to_player_history(dir);
		}
	} catch (ProjectFolderError &) {
		error_dialog(
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
		error_dialog(this, wxString::Format(_("Could not load a DCP from %s"), std_to_wx(dir.string())), std_to_wx(e.what()));
	} catch (DCPError& e) {
		error_dialog(this, wxString::Format(_("Could not load a DCP from %s"), std_to_wx(dir.string())), std_to_wx(e.what()));
	}
}


/* _film is now something new: set up to play it */
void
PlayerFrame::prepare_to_play_film(optional<float> crop_to_ratio)
{
	if (_viewer.playing()) {
		_viewer.stop();
	}

	/* Start off as Flat */
	auto auto_ratio = Ratio::from_id("185");

	_film->set_audio_channels(MAX_DCP_AUDIO_CHANNELS);

	for (auto i: _film->content()) {
		auto dcp = dynamic_pointer_cast<DCPContent>(i);

		copy_dcp_markers_to_film(dcp, _film);

		for (auto j: i->text) {
			j->set_use(true);
		}

		if (i->video && i->video->size()) {
			auto const r = Ratio::nearest_from_ratio(i->video->size()->ratio());
			if (r.id() == "239") {
				/* Any scope content means we use scope */
				auto_ratio = r;
			}
		}

		/* Any 3D content means we use 3D mode */
		if (i->video && i->video->frame_type() != VideoFrameType::TWO_D) {
			_film->set_three_d(true);
		}

		if (dcp->video_frame_rate()) {
			_film->set_video_frame_rate(dcp->video_frame_rate().get());
		}

		switch (dcp->video_encoding().get_value_or(VideoEncoding::JPEG2000)) {
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

	set_audio_delay_from_config();

	auto old = _cpl_menu->GetMenuItems();
	for (auto const& i: old) {
		_cpl_menu->Remove(i);
	}

	if (_film->content().size() == 1) {
		/* Offer a CPL menu */
		if (auto first = dynamic_pointer_cast<DCPContent>(_film->content().front())) {
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

		if (crop_to_ratio) {
			auto size = _film->content()[0]->video->size().get_value_or({1998, 1080});
			int pixels = 0;
			if (*crop_to_ratio > (2048.0 / 1080.0)) {
				pixels = (size.height - (size.width / *crop_to_ratio)) / 2;
				_film->content()[0]->video->set_crop(Crop{0, 0, std::max(0, pixels), std::max(0, pixels)});
			} else {
				pixels = (size.width - (size.height * *crop_to_ratio)) / 2;
				_film->content()[0]->video->set_crop(Crop{std::max(0, pixels), std::max(0, pixels), 0, 0});
			}
		}
	}

	if (crop_to_ratio) {
		_film->set_container(Ratio(*crop_to_ratio, "custom", "custom", {}, "custom"));
	} else {
		_film->set_container(auto_ratio);
	}

	_viewer.set_film(_film);
	_viewer.seek(DCPTime(), true);
	_viewer.set_eyes(_view_eye_left->IsChecked() ? Eyes::LEFT : Eyes::RIGHT);
	_info->triggered_update();
	set_menu_sensitivity();

	_controls->set_film(_film);
}


void
PlayerFrame::set_audio_delay_from_config()
{
	for (auto content: _playlist) {
		if (content.first->audio) {
			content.first->audio->set_delay(Config::instance()->player_audio_delay());
		}
	}
}


void
PlayerFrame::load_stress_script(boost::filesystem::path path)
{
	_stress.load_script(path);
}


void
PlayerFrame::idle()
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


void
PlayerFrame::examine_content()
{
	if (_playlist.empty()) {
		return;
	}

	auto dcp = dynamic_pointer_cast<DCPContent>(_playlist.front().first);
	DCPOMATIC_ASSERT(dcp);
	dcp->examine({}, true);

	/* Examining content re-creates the TextContent objects, so we must re-enable them */
	for (auto i: dcp->text) {
		i->set_use(true);
	}
}


bool
PlayerFrame::report_errors_from_last_job(wxWindow* parent) const
{
	auto jm = JobManager::instance();

	DCPOMATIC_ASSERT(!jm->get().empty());

	auto last = jm->get().back();
	if (last->finished_in_error()) {
		error_dialog(parent, wxString::Format(_("Could not load DCP.\n\n%s."), std_to_wx(last->error_summary()).data()), std_to_wx(last->error_details()));
		return false;
	}

	return true;
}


void
PlayerFrame::setup_menu(wxMenuBar* m)
{
	_file_menu = new wxMenu;
	_file_menu->Append(ID_file_open, _("&Open...\tCtrl-O"));
	_file_add_ov = _file_menu->Append(ID_file_add_ov, _("&Add OV..."));
	_file_add_kdm = _file_menu->Append(ID_file_add_kdm, _("Add &KDM..."));
	_file_menu->AppendSeparator();
	_file_save_frame = _file_menu->Append(ID_file_save_frame, _("&Save frame to file...\tCtrl-S"));

	_history_position = _file_menu->GetMenuItems().GetCount();

	_file_menu->AppendSeparator();
	_file_menu->Append(ID_file_close, _("&Close"));
	_file_menu->AppendSeparator();

#ifdef __WXOSX__
	_file_menu->Append(wxID_EXIT, _("&Exit"));
#else
	_file_menu->Append(wxID_EXIT, _("&Quit"));
#endif

#ifdef __WXOSX__
	auto prefs = _file_menu->Append(wxID_PREFERENCES, _("&Preferences...\tCtrl-,"));
#else
	auto edit = new wxMenu;
	auto prefs = edit->Append(wxID_PREFERENCES, _("&Preferences...\tCtrl-P"));
#endif

	prefs->Enable(Config::instance()->have_write_permission());

	_cpl_menu = new wxMenu;

	auto view = new wxMenu;
	auto c = Config::instance()->decode_reduction();
	_view_cpl = view->Append(ID_view_cpl, _("CPL"), _cpl_menu);
	view->AppendSeparator();
	_view_full_screen = view->AppendCheckItem(ID_view_full_screen, _("Full screen\tF11"));
	_view_dual_screen = view->AppendCheckItem(ID_view_dual_screen, _("Dual screen\tShift+F11"));
	setup_menu();
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
	_tools_verify = tools->Append(ID_tools_verify, _("Verify DCP..."));
	_tools_audio_graph = tools->Append(ID_tools_audio_graph, _("Audio graph..."));
	tools->AppendSeparator();
	tools->Append(ID_tools_check_for_updates, _("Check for updates"));
	tools->Append(ID_tools_timing, _("Timing..."));
	tools->Append(ID_tools_system_information, _("System information..."));

	auto help = new wxMenu;
#ifdef __WXOSX__
	help->Append(wxID_ABOUT, variant::wx::insert_dcpomatic_player(_("About %s")));
#else
	help->Append(wxID_ABOUT, _("About"));
#endif
	if (variant::show_report_a_problem()) {
		help->Append(ID_help_report_a_problem, _("Report a problem..."));
	}

	m->Append (_file_menu, _("&File"));
	if (!Config::instance()->player_restricted_menus()) {
#ifndef __WXOSX__
		m->Append(edit, _("&Edit"));
#endif
		m->Append(view, _("&View"));
		m->Append(tools, _("&Tools"));
		m->Append(help, _("&Help"));
	}
}


void
PlayerFrame::file_open()
{
	auto d = wxStandardPaths::Get().GetDocumentsDir();
	if (Config::instance()->last_player_load_directory()) {
		d = std_to_wx(Config::instance()->last_player_load_directory()->string());
	}

	wxDirDialog dialog(this, _("Select DCP to open"), d, wxDEFAULT_DIALOG_STYLE | wxDD_DIR_MUST_EXIST);

	int r;
	while (true) {
		r = dialog.ShowModal();
		if (r == wxID_OK && dialog.GetPath() == wxStandardPaths::Get().GetDocumentsDir()) {
			error_dialog(this, _("You did not select a folder.  Make sure that you select a folder before clicking Open."));
		} else {
			break;
		}
	}

	if (r == wxID_OK) {
		boost::filesystem::path const dcp(wx_to_std(dialog.GetPath()));
		load_dcp(dcp);
		Config::instance()->set_last_player_load_directory(dcp.parent_path());
	}
}


void
PlayerFrame::file_add_ov()
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
			error_dialog(this, _("You did not select a folder.  Make sure that you select a folder before clicking Open."));
		} else {
			break;
		}
	}

	if (r == wxID_OK) {
		DCPOMATIC_ASSERT(!_playlist.empty());
		auto dcp = std::dynamic_pointer_cast<DCPContent>(_playlist.front().first);
		DCPOMATIC_ASSERT(dcp);

		try {
			dcp->add_ov(wx_to_std(dialog.GetPath()));
		} catch (DCPError& e) {
			error_dialog(this, char_to_wx(e.what()));
			return;
		}

		auto job = make_shared<ExamineContentJob>(vector<shared_ptr<Content>>{dcp}, true);
		_examine_job_connection = job->Finished.connect(boost::bind(&PlayerFrame::take_playlist_entry, this));
		JobManager::instance()->add(job);

		display_progress(variant::wx::dcpomatic_player(), _("Loading content"));
		report_errors_from_last_job(this);
	}
}


void
PlayerFrame::file_add_kdm()
{
	FileDialog dialog(this, _("Select KDM"), char_to_wx("XML files|*.xml|All files|*.*"), wxFD_MULTIPLE, "AddKDMPath");

	if (dialog.show()) {
		DCPOMATIC_ASSERT(!_playlist.empty());
		auto dcp = std::dynamic_pointer_cast<DCPContent>(_playlist.front().first);
		DCPOMATIC_ASSERT(dcp);
		try {
			dcp::ScopeGuard sg([this]() {
				_viewer.set_coalesce_player_changes(false);
			});
			_viewer.set_coalesce_player_changes(true);
			for (auto path: dialog.paths()) {
				dcp->add_kdm(dcp::EncryptedKDM(dcp::file_to_string(path)));
				_kdms.push_back(path);
			}
			examine_content();
		} catch (exception& e) {
			error_dialog(this, wxString::Format(_("Could not load KDM.")), std_to_wx(e.what()));
			return;
		}
	}

	_info->triggered_update();
	set_menu_sensitivity();
}


void
PlayerFrame::file_save_frame()
{
	wxFileDialog dialog(this, _("Save frame to file"), {}, {}, char_to_wx("PNG files (*.png)|*.png|JPEG files (*.jpg;*.jpeg)|*.jpg;*.jpeg"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
	if (dialog.ShowModal() == wxID_CANCEL) {
		return;
	}

	auto path = boost::filesystem::path(wx_to_std(dialog.GetPath()));

	auto player = make_shared<Player>(_film, Image::Alignment::PADDED, true);
	player->seek(_viewer.position(), true);

	bool done = false;
	player->Video.connect([path, &done, this](shared_ptr<PlayerVideo> video, DCPTime) {
		auto ext = boost::algorithm::to_lower_copy(path.extension().string());
		if (ext == ".png") {
			auto image = video->image(force(AV_PIX_FMT_RGBA), VideoRange::FULL, false);
			image_as_png(image).write(path);
		} else if (ext == ".jpg" || ext == ".jpeg") {
			auto image = video->image(force(AV_PIX_FMT_RGB24), VideoRange::FULL, false);
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

	DCPOMATIC_ASSERT(tries_left >= 0);
}


void
PlayerFrame::file_history(wxCommandEvent& event)
{
	auto history = Config::instance()->player_history();
	int n = event.GetId() - ID_file_history;
	if (n >= 0 && n < static_cast<int>(history.size())) {
		try {
			load_dcp(history[n]);
		} catch (exception& e) {
			error_dialog(nullptr, wxString::Format(_("Could not load DCP %s."), std_to_wx(history[n].string()))), std_to_wx(e.what());
		}
	}
}


void
PlayerFrame::file_close()
{
	_playlist.clear();
	_playlist_position = 0;
	_controls->playlist_changed();

	take_playlist_entry();
	_info->triggered_update();
	set_menu_sensitivity();
}


void
PlayerFrame::file_exit()
{
	Close();
}


void
PlayerFrame::edit_preferences()
{
	if (!Config::instance()->have_write_permission()) {
		return;
	}

	if (!_config_dialog) {
		_config_dialog = create_player_config_dialog();
	}
	_config_dialog->Show(this);
}


void
PlayerFrame::view_cpl(wxCommandEvent& ev)
{
	DCPOMATIC_ASSERT(!_playlist.empty());
	auto dcp = std::dynamic_pointer_cast<DCPContent>(_playlist.front().first);
	DCPOMATIC_ASSERT(dcp);
	auto cpls = dcp->cpls();
	int id = ev.GetId() - ID_view_cpl;
	DCPOMATIC_ASSERT(id >= 0);
	DCPOMATIC_ASSERT(id < int(cpls.size()));
	auto i = cpls.begin();
	while (id > 0) {
		++i;
		--id;
	}

	_viewer.set_coalesce_player_changes(true);
	dcp->set_cpl(*i);
	examine_content();
	_viewer.set_coalesce_player_changes(false);

	_info->triggered_update();
}


void
PlayerFrame::view_eye_changed(wxCommandEvent& ev)
{
	_viewer.set_eyes(ev.GetId() == ID_view_eye_left ? Eyes::LEFT : Eyes::RIGHT);
}


void
PlayerFrame::view_full_screen()
{
	if (_mode == Config::PlayerMode::FULL) {
		_mode = Config::PlayerMode::WINDOW;
	} else {
		_mode = Config::PlayerMode::FULL;
	}
	setup_screen();
	setup_menu();
}


void
PlayerFrame::view_dual_screen()
{
	if (_mode == Config::PlayerMode::DUAL) {
		_mode = Config::PlayerMode::WINDOW;
	} else {
		_mode = Config::PlayerMode::DUAL;
	}
	setup_screen();
	setup_menu();
}


void
PlayerFrame::setup_menu()
{
	if (_view_full_screen) {
		_view_full_screen->Check(_mode == Config::PlayerMode::FULL);
	}
	if (_view_dual_screen) {
		_view_dual_screen->Check(_mode == Config::PlayerMode::DUAL);
	}
}


void
PlayerFrame::setup_screen()
{
	_controls->Show(_mode != Config::PlayerMode::FULL);
	_info->Show(_mode != Config::PlayerMode::FULL);
	_overall_panel->SetBackgroundColour(_mode == Config::PlayerMode::FULL ? wxColour(0, 0, 0) : wxNullColour);
	ShowFullScreen(_mode == Config::PlayerMode::FULL);
	_viewer.set_pad_black(_mode != Config::PlayerMode::WINDOW);

	if (_mode == Config::PlayerMode::DUAL) {
		_dual_screen = new wxFrame(this, wxID_ANY, {});
		_dual_screen->SetBackgroundColour(wxColour(0, 0, 0));
		_dual_screen->ShowFullScreen(true);
		_viewer.panel()->Reparent(_dual_screen);
		_viewer.panel()->SetFocus();
		_dual_screen->Show();
		LOG_DEBUG_PLAYER("Setting up dual screen mode with {} displays", wxDisplay::GetCount());
		for (auto index = 0U; index < wxDisplay::GetCount(); ++index) {
			wxDisplay display(index);
			auto client = display.GetClientArea();
			auto mode = display.GetCurrentMode();
			auto geometry = display.GetGeometry();
			LOG_DEBUG_PLAYER("Display {}", index);
			LOG_DEBUG_PLAYER("  ClientArea position=({}, {}) size=({}, {})", client.GetX(), client.GetY(), client.GetWidth(), client.GetHeight());
			LOG_DEBUG_PLAYER("  Geometry   position=({}, {}) size=({}, {})", geometry.GetX(), geometry.GetY(), geometry.GetWidth(), geometry.GetHeight());
			LOG_DEBUG_PLAYER("  Mode       size=({}, {})", mode.GetWidth(), mode.GetHeight());
			LOG_DEBUG_PLAYER("  Primary?   {}", static_cast<int>(display.IsPrimary()));
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
		_dual_screen->Bind(wxEVT_CHAR_HOOK, boost::bind(&PlayerFrame::dual_screen_key_press, this, _1));
	} else {
		if (_dual_screen) {
			_viewer.panel()->Reparent(_overall_panel);
			_dual_screen->Destroy();
			_dual_screen = 0;
		}
	}

	setup_main_sizer(_mode);
}


void
PlayerFrame::dual_screen_key_press(wxKeyEvent& ev)
{
	if (ev.GetKeyCode() == WXK_F11) {
		if (ev.ShiftDown()) {
			view_dual_screen();
		} else if (!ev.HasAnyModifiers()) {
			view_full_screen();
		}
	}
}


void
PlayerFrame::view_closed_captions()
{
	_viewer.show_closed_captions();
}


void
PlayerFrame::tools_verify()
{
	DCPOMATIC_ASSERT(!_playlist.empty());
	auto dcp = std::dynamic_pointer_cast<DCPContent>(_playlist.front().first);
	DCPOMATIC_ASSERT(dcp);

	VerifyDCPDialog dialog(this, _("Verify DCP"), dcp->directories(), _kdms);
	dialog.ShowModal();
}


void
PlayerFrame::tools_audio_graph()
{
	DCPOMATIC_ASSERT(!_playlist.empty());
	auto dcp = std::dynamic_pointer_cast<DCPContent>(_playlist.front().first);
	DCPOMATIC_ASSERT(dcp);

	_audio_dialog.reset(this, _film, dcp);
	_audio_dialog->Seek.connect(boost::bind(&FilmViewer::seek, &_viewer, _1, true));
	_audio_dialog->Show();
}


void
PlayerFrame::tools_check_for_updates()
{
	UpdateChecker::instance()->run();
	_update_news_requested = true;
}


void
PlayerFrame::tools_timing()
{
	TimerDisplay dialog(this, _viewer.state_timer(), _viewer.gets());
	dialog.ShowModal();
}


void
PlayerFrame::tools_system_information()
{
	if (!_system_information_dialog) {
		_system_information_dialog.reset(this, _viewer);
	}

	_system_information_dialog->Show();
}


void
PlayerFrame::help_about()
{
	AboutDialog dialog(this);
	dialog.ShowModal();
}


void
PlayerFrame::help_report_a_problem()
{
	ReportProblemDialog dialog(this);
	if (dialog.ShowModal() == wxID_OK) {
		dialog.report();
	}
}


void
PlayerFrame::update_checker_state_changed()
{
	auto uc = UpdateChecker::instance();

	bool const announce =
		_update_news_requested ||
		(uc->stable() && Config::instance()->check_for_updates()) ||
		(uc->test() && Config::instance()->check_for_updates() && Config::instance()->check_for_test_updates());

	_update_news_requested = false;

	if (!announce) {
		return;
	}

	if (uc->state() == UpdateChecker::State::YES) {
		UpdateDialog dialog(this, uc->stable(), uc->test());
		dialog.ShowModal();
	} else if (uc->state() == UpdateChecker::State::FAILED) {
		error_dialog(this, variant::wx::insert_dcpomatic(_("The %s download server could not be contacted.")));
	} else {
		error_dialog(this, variant::wx::insert_dcpomatic(_("There are no new versions of %s available.")));
	}

	_update_news_requested = false;
}


void
PlayerFrame::config_changed(Config::Property prop)
{
	/* Instantly save any config changes when using the player GUI */
	try {
		Config::instance()->write_config();
	} catch (FileError& e) {
		if (prop != Config::HISTORY) {
			error_dialog(
				this,
				wxString::Format(
					_("Could not write to config file at %s.  Your changes have not been saved."),
					std_to_wx(e.file().string())
					)
				);
		}
	} catch (exception& e) {
		error_dialog(
			this,
			_("Could not write to config file.  Your changes have not been saved.")
			);
	}

	update_from_config(prop);

	setup_http_server();
}


void
PlayerFrame::stop_http_server()
{
	if (_http_server) {
		_http_server->stop();
		_http_server_thread.join();
		_http_server.reset();
	}
}


void
PlayerFrame::setup_http_server()
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
		LOG_DEBUG_PLAYER("Failed to start player HTTP server ({})", e.what());
	}
}


void
PlayerFrame::setup_internal_player_server()
{
	try {
		auto server = new InternalPlayerServer();
		server->LoadDCP.connect(boost::bind(&PlayerFrame::load_dcp, this, _1));
		new thread(boost::bind(&InternalPlayerServer::run, server));
	} catch (std::exception& e) {
		/* This is not the end of the world; probably a failure to bind the server socket
		 * because there's already another player running.
		 */
		LOG_DEBUG_PLAYER("Failed to start internal player server ({})", e.what());
	}
}


void
PlayerFrame::update_from_config(Config::Property prop)
{
	for (int i = 0; i < _history_items; ++i) {
		delete _file_menu->Remove(ID_file_history + i);
	}

	if (_history_separator) {
		_file_menu->Remove(_history_separator);
	}
	delete _history_separator;
	_history_separator = nullptr;

	int pos = _history_position;

	/* Clear out non-existent history items before we re-build the menu */
	Config::instance()->clean_player_history();
	auto history = Config::instance()->player_history();

	if (!history.empty()) {
		_history_separator = _file_menu->InsertSeparator(pos++);
	}

	for (size_t i = 0; i < history.size(); ++i) {
		string s;
		if (i < 9) {
			s = fmt::format("&{} {}", i + 1, history[i].string());
		} else {
			s = history[i].string();
		}
		_file_menu->Insert(pos++, ID_file_history + i, std_to_wx(s));
	}

	_history_items = history.size();

	if (prop == Config::PLAYER_DEBUG_LOG) {
		auto p = Config::instance()->player_debug_log_file();
		if (p) {
			dcpomatic_log = make_shared<FileLog>(*p);
		} else {
			dcpomatic_log = make_shared<NullLog>();
		}
	}

	dcpomatic_log->set_types(Config::instance()->log_types());

	set_audio_delay_from_config();
}


void
PlayerFrame::set_menu_sensitivity()
{
	auto const have_content = !_playlist.empty();
	auto const dcp = _viewer.dcp();
	auto const playable = dcp && !dcp->needs_assets() && !dcp->needs_kdm();
	_tools_verify->Enable(have_content);
	_tools_audio_graph->Enable(playable);
	_file_add_ov->Enable(have_content);
	_file_add_kdm->Enable(have_content);
	_file_save_frame->Enable(playable);
	_view_cpl->Enable(have_content);
	_view_eye->Enable(have_content && _film->three_d());
}


void
PlayerFrame::start_stop_pressed()
{
	if (_viewer.playing()) {
		_viewer.stop();
	} else {
		_viewer.start();
	}
}


void
PlayerFrame::go_back_frame()
{
	_viewer.seek_by(-_viewer.one_video_frame(), true);
}


void
PlayerFrame::go_forward_frame()
{
	_viewer.seek_by(_viewer.one_video_frame(), true);
}


void
PlayerFrame::go_seconds(int s)
{
	_viewer.seek_by(DCPTime::from_seconds(s), true);
}


void
PlayerFrame::go_to_start()
{
	_viewer.seek(DCPTime(), true);
}


void
PlayerFrame::go_to_end()
{
	_viewer.seek(_film->length() - _viewer.one_video_frame(), true);
}

static
optional<dcp::EncryptedKDM>
get_kdm_from_directory(shared_ptr<DCPContent> dcp)
{
	using namespace boost::filesystem;
	auto kdm_dir = Config::instance()->player_kdm_directory();
	if (!kdm_dir) {
		return {};
	}
	for (auto i: directory_iterator(*kdm_dir)) {
		try {
			if (file_size(i.path()) < MAX_KDM_SIZE) {
				dcp::EncryptedKDM kdm(dcp::file_to_string(i.path()));
				if (kdm.cpl_id() == dcp->cpl()) {
					return kdm;
				}
			}
		} catch (std::exception& e) {
			/* Hey well */
		}
	}

	return {};
}


bool
PlayerFrame::set_playlist(vector<ShowPlaylistEntry> playlist)
{
	bool was_playing = false;
	if (_viewer.playing()) {
		was_playing = true;
		_viewer.stop();
	}

	wxProgressDialog dialog(variant::wx::dcpomatic(), _("Loading playlist and KDMs"));

	_playlist.clear();
	_playlist_position = 0;

	auto const store = ShowPlaylistContentStore::instance();
	for (auto const& entry: playlist) {
		dialog.Pulse();
		auto content = store->get(entry);
		if (!content) {
			error_dialog(this, _("This playlist cannot be loaded as some content is missing."));
			_playlist.clear();
			_controls->playlist_changed();
			return false;
		}

		auto dcp = dynamic_pointer_cast<DCPContent>(content);
		if (dcp && dcp->needs_kdm()) {
			optional<dcp::EncryptedKDM> kdm;
			kdm = get_kdm_from_directory(dcp);
			if (kdm) {
				try {
					dcp->add_kdm(*kdm);
					dcp->examine(shared_ptr<Job>(), true);
				} catch (KDMError& e) {
					error_dialog(this, _("Could not load KDM."));
				}
			}
			if (dcp->needs_kdm()) {
				/* We didn't get a KDM for this */
				error_dialog(this, _("This playlist cannot be loaded as a KDM is missing or incorrect."));
				_playlist.clear();
				_controls->playlist_changed();
				return false;
			}
		}
		_playlist.push_back({content, entry.crop_to_ratio()});
	}

	take_playlist_entry();

	if (was_playing) {
		_viewer.start();
	}

	_controls->playlist_changed();

	return true;
}


/** Stop the viewer, take the thing at _playlist_position and prepare to play it.
 *  Set up to play nothing if the playlist is empty, or we're off the
 *  end of it.
 *  @return true if the viewer was playing when this method was called.
 */
bool
PlayerFrame::take_playlist_entry()
{
	boost::optional<float> crop_to_ratio;

	if (_playlist_position < 0 || _playlist_position >= static_cast<int>(_playlist.size())) {
		_film = std::make_shared<Film>(boost::none);
	} else {
		auto const entry = _playlist[_playlist_position];

		_film = std::make_shared<Film>(optional<boost::filesystem::path>());
		_film->add_content({entry.first});

		if (!entry.second) {
			crop_to_ratio = Config::instance()->player_crop_output_ratio();
		}
	}

	bool const playing = _viewer.playing();
	if (playing) {
		_viewer.stop();
	}

	/* Start off as Flat */
	auto auto_ratio = Ratio::from_id("185");

	_film->set_audio_channels(MAX_DCP_AUDIO_CHANNELS);

	for (auto i: _film->content()) {
		auto dcp = dynamic_pointer_cast<DCPContent>(i);

		copy_dcp_markers_to_film(dcp, _film);

		for (auto j: i->text) {
			j->set_use(true);
		}

		if (i->video && i->video->size()) {
			auto const r = Ratio::nearest_from_ratio(i->video->size()->ratio());
			if (r.id() == "239") {
				/* Any scope content means we use scope */
				auto_ratio = r;
			}
		}

		/* Any 3D content means we use 3D mode */
		if (i->video && i->video->frame_type() != VideoFrameType::TWO_D) {
			_film->set_three_d(true);
		}

		if (dcp->video_frame_rate()) {
			_film->set_video_frame_rate(dcp->video_frame_rate().get());
		}

		switch (dcp->video_encoding().get_value_or(VideoEncoding::JPEG2000)) {
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

	set_audio_delay_from_config();

	auto old = _cpl_menu->GetMenuItems();
	for (auto const& i: old) {
		_cpl_menu->Remove(i);
	}

	if (_film->content().size() == 1) {
		/* Offer a CPL menu */
		if (auto first = dynamic_pointer_cast<DCPContent>(_film->content().front())) {
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

		if (crop_to_ratio) {
			auto size = _film->content()[0]->video->size().get_value_or({1998, 1080});
			int pixels = 0;
			if (*crop_to_ratio > (2048.0 / 1080.0)) {
				pixels = (size.height - (size.width / *crop_to_ratio)) / 2;
				_film->content()[0]->video->set_crop(Crop{0, 0, std::max(0, pixels), std::max(0, pixels)});
			} else {
				pixels = (size.width - (size.height * *crop_to_ratio)) / 2;
				_film->content()[0]->video->set_crop(Crop{std::max(0, pixels), std::max(0, pixels), 0, 0});
			}
		}
	}

	if (crop_to_ratio) {
		_film->set_container(Ratio(*crop_to_ratio, "custom", "custom", {}, "custom"));
	} else {
		_film->set_container(auto_ratio);
	}

	_viewer.set_film(_film);
	_viewer.seek(DCPTime(), true);
	_viewer.set_eyes(_view_eye_left->IsChecked() ? Eyes::LEFT : Eyes::RIGHT);
	_info->triggered_update();
	set_menu_sensitivity();

	_controls->set_film(_film);
	return playing;
}


void
PlayerFrame::viewer_finished()
{
	_playlist_position++;

	/* Either get the next piece of content, or go black */
	take_playlist_entry();

	if (_playlist_position < static_cast<int>(_playlist.size())) {
		/* Start the next piece of content */
		_viewer.start();
	} else {
		/* Be ready to start again from the top of the playlist */
		_playlist_position = 0;
	}
}



bool
PlayerFrame::can_do_next() const
{
	return _playlist_position < (static_cast<int>(_playlist.size()) - 1);
}


void
PlayerFrame::next()
{
	_playlist_position++;
	if (take_playlist_entry()) {
		_viewer.start();
	}
}


bool
PlayerFrame::can_do_previous() const
{
	return _playlist_position > 0;
}


void
PlayerFrame::previous()
{
	_playlist_position--;
	if (take_playlist_entry()) {
		_viewer.start();
	}
}


vector<shared_ptr<Content>>
PlayerFrame::playlist() const
{
	vector<shared_ptr<Content>> content;
	for (auto entry: _playlist) {
		content.push_back(entry.first);
	}
	return content;
}


