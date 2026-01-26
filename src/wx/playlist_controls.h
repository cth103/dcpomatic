/*
    Copyright (C) 2018-2020 Carl Hetherington <cth@carlh.net>

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


#include "controls.h"
#include "lib/show_playlist.h"
#include "lib/show_playlist_id.h"


class DCPContent;
class ShowPlaylistList;

namespace dcpomatic {
namespace ui {

class PlayerFrame;

}
}


class PlaylistControls : public Controls
{
public:
	PlaylistControls(wxWindow* parent, dcpomatic::ui::PlayerFrame* player, FilmViewer& viewer);

	void playlist_changed() override;

private:
	void play_clicked();
	void pause_clicked();
	void stop_clicked();
	void next_clicked();
	void previous_clicked();

	void update_playlists();
	void playlist_selection_changed();

	void config_changed(int) override;
	void started() override;
	void stopped() override;

	void setup_sensitivity() override;

	void add_playlist_to_list(ShowPlaylist spl);
	void add_next_playlist_entry(ShowPlaylistEntry entry);
	void clear_next_playlist();
	void content_activated(std::weak_ptr<Content> content);

	boost::optional<dcp::EncryptedKDM> get_kdm_from_directory(std::shared_ptr<DCPContent> dcp);

	dcpomatic::ui::PlayerFrame* _player;

	wxButton* _play_button;
	wxButton* _pause_button;
	wxButton* _stop_button;
	wxButton* _next_button;
	wxButton* _previous_button;

	ContentView* _content_view;
	wxButton* _refresh_content_view;
	wxListCtrl* _playlists_view;
	wxButton* _refresh_playlists_view;
	wxListCtrl* _next_playlist_view;
	wxButton* _clear_next_playlist;

	wxListCtrl* _current_playlist_view;

	std::unique_ptr<ShowPlaylistList> _playlists;
	std::vector<ShowPlaylistEntry> _next_playlist;

	bool _paused = false;
};
