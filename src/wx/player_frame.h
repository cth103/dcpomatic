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


#include "audio_dialog.h"
#include "film_viewer.h"
#include "player_stress_tester.h"
#include "system_information_dialog.h"
#include "wx_ptr.h"
#include "lib/config.h"
#include "lib/http_server.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/dnd.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <boost/signals2.hpp>
#include <boost/filesystem.hpp>


class wxPreferencesEditor;
class PlayerInformation;


namespace dcpomatic {
namespace ui {


class PlayerFrame : public wxFrame
{
public:
	class DCPDropTarget : public wxFileDropTarget
	{
	public:
		DCPDropTarget(PlayerFrame* owner);
		bool OnDropFiles(wxCoord, wxCoord, wxArrayString const& filenames) override;

	private:
		PlayerFrame* _frame;
	};

	PlayerFrame();
	~PlayerFrame();

	void close(wxCloseEvent& ev);
	void setup_main_sizer(Config::PlayerMode mode);
	bool playback_permitted();
	void too_many_frames_dropped();
	void set_decode_reduction(boost::optional<int> reduction);
	void load_dcp(boost::filesystem::path dir);
	void reset_film_weak(std::weak_ptr<Film> weak_film, boost::optional<float> crop_to_ratio);
	void reset_film(std::shared_ptr<Film> film = std::make_shared<Film>(boost::none), boost::optional<float> crop_to_ratio = {});

	/* _film is now something new: set up to play it */
	void prepare_to_play_film(boost::optional<float> crop_to_ratio);
	void set_audio_delay_from_config();
	void load_stress_script(boost::filesystem::path path);
	void idle();

private:
	void examine_content();
	bool report_errors_from_last_job(wxWindow* parent) const;
	void setup_menu(wxMenuBar* m);
	void file_open();
	void file_add_ov();
	void file_add_kdm();
	void file_save_frame();
	void file_history(wxCommandEvent& event);
	void file_close();
	void file_exit();
	void edit_preferences();
	void view_cpl(wxCommandEvent& ev);
	void view_eye_changed(wxCommandEvent& ev);
	void view_full_screen();
	void view_dual_screen();
	void setup_menu();
	void setup_screen();
	void dual_screen_key_press(wxKeyEvent& ev);
	void view_closed_captions();
	void tools_verify();
	void tools_audio_graph();
	void tools_check_for_updates();
	void tools_timing();
	void tools_system_information();
	void help_about();
	void help_report_a_problem();

	void update_checker_state_changed();
	void config_changed(Config::Property prop);
	void stop_http_server();
	void setup_http_server();
	void setup_internal_player_server();
	void update_from_config(Config::Property prop);
	void set_menu_sensitivity();
	void start_stop_pressed();
	void go_back_frame();
	void go_forward_frame();
	void go_seconds(int s);
	void go_to_start();
	void go_to_end();

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
	wx_ptr<SystemInformationDialog> _system_information_dialog;
	std::shared_ptr<Film> _film;
	boost::signals2::scoped_connection _config_changed_connection;
	boost::signals2::scoped_connection _examine_job_connection;
	wxMenuItem* _file_add_ov = nullptr;
	wxMenuItem* _file_add_kdm = nullptr;
	wxMenuItem* _file_save_frame = nullptr;
	wxMenuItem* _tools_verify = nullptr;
	wxMenuItem* _tools_audio_graph = nullptr;
	wxMenuItem* _view_full_screen = nullptr;
	wxMenuItem* _view_dual_screen = nullptr;
	wxSizer* _main_sizer = nullptr;
	PlayerStressTester _stress;
	/** KDMs that have been loaded, so that we can pass them to the verifier */
	std::vector<boost::filesystem::path> _kdms;
	boost::thread _http_server_thread;
	std::unique_ptr<HTTPServer> _http_server;
	struct timeval _last_http_server_update = { 0, 0 };
	wx_ptr<AudioDialog> _audio_dialog;
};


}
}


