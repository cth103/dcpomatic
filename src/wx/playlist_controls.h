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
#include "lib/spl.h"

class DCPContent;

class PlaylistControls : public Controls
{
public:
	PlaylistControls (wxWindow* parent, std::shared_ptr<FilmViewer> viewer);

	void log (wxString s);
	void set_film (std::shared_ptr<Film> film);

	/** This is so that we can tell our parent player to reset the film
	    when we have created one from a SPL.  We could call a method
	    in the player's DOMFrame but we don't have that in a header.
	*/
	boost::signals2::signal<void (std::weak_ptr<Film>)> ResetFilm;

	void play ();
	void stop ();

private:
	void play_clicked ();
	void pause_clicked ();
	void stop_clicked ();
	void next_clicked ();
	void previous_clicked ();
	void add_playlist_to_list (SPL spl);
	void update_content_directory ();
	void update_playlist_directory ();
	void spl_selection_changed ();
	void select_playlist (int selected, int position);
	void started ();
	void stopped ();
	void setup_sensitivity ();
	void config_changed (int);
	void viewer_finished ();
	void reset_film ();
	void update_current_content ();
	bool can_do_previous ();
	bool can_do_next ();
	void deselect_playlist ();

	boost::optional<dcp::EncryptedKDM> get_kdm_from_directory (std::shared_ptr<DCPContent> dcp);

	wxButton* _play_button;
	wxButton* _pause_button;
	wxButton* _stop_button;
	wxButton* _next_button;
	wxButton* _previous_button;

	ContentView* _content_view;
	wxButton* _refresh_content_view;
	wxListCtrl* _spl_view;
	wxButton* _refresh_spl_view;
	wxListCtrl* _current_spl_view;

	std::vector<SPL> _playlists;
	boost::optional<int> _selected_playlist;
	int _selected_playlist_position;
};
