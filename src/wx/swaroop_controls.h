/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

class SwaroopControls : public Controls
{
public:
	SwaroopControls (wxWindow* parent, boost::shared_ptr<FilmViewer> viewer);

	void log (wxString s);
	void set_film (boost::shared_ptr<Film> film);

	/** This is so that we can tell our parent player to reset the film
	    when we have created one from a SPL.  We could call a method
	    in the player's DOMFrame but we don't have that in a header.
	*/
	boost::signals2::signal<void (boost::weak_ptr<Film>)> ResetFilm;

private:
	void play_clicked ();
	void pause_clicked ();
	void stop_clicked ();
	void add_playlist_to_list (SPL spl);
	void update_content_directory ();
	void update_playlist_directory ();
	void spl_selection_changed ();
	void started ();
	void stopped ();
	void setup_sensitivity ();
	void config_changed (int);
	void viewer_finished ();
	void reset_film ();

	wxButton* _play_button;
	wxButton* _pause_button;
	wxButton* _stop_button;

	ContentView* _content_view;
	wxButton* _refresh_content_view;
	wxListCtrl* _spl_view;
	wxButton* _refresh_spl_view;
	wxListCtrl* _current_spl_view;
	wxTextCtrl* _log;

	bool _current_disable_timeline;

	std::vector<SPL> _playlists;
	boost::optional<int> _selected_playlist;
	int _selected_playlist_position;
};
