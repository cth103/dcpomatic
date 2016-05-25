/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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

/** @file  src/film_viewer.h
 *  @brief A wx widget to view `thumbnails' of a Film.
 */

#include <wx/wx.h>
#include "lib/film.h"

class wxToggleButton;
class FFmpegPlayer;
class Image;
class RGBPlusAlphaImage;
class PlayerVideo;

/** @class FilmViewer
 *  @brief A wx widget to view a preview of a Film.
 */
class FilmViewer : public wxPanel
{
public:
	FilmViewer (wxWindow *);

	void set_film (boost::shared_ptr<Film>);

	DCPTime position () const {
		return _position;
	}

	void set_position (DCPTime p);
	void set_coalesce_player_changes (bool c);

	void refresh ();

	boost::signals2::signal<void (boost::weak_ptr<PlayerVideo>)> ImageChanged;

private:
	void paint_panel ();
	void panel_sized (wxSizeEvent &);
	void slider_moved ();
	void play_clicked ();
	void timer ();
	void calculate_sizes ();
	void check_play_state ();
	void active_jobs_changed (boost::optional<std::string>);
	void back_clicked ();
	void forward_clicked ();
	void player_changed (bool);
	void update_position_label ();
	void update_position_slider ();
	void get (DCPTime, bool);
	void refresh_panel ();
	void setup_sensitivity ();
	void film_changed (Film::Property);

	boost::shared_ptr<Film> _film;
	boost::shared_ptr<Player> _player;

	wxSizer* _v_sizer;
	wxPanel* _panel;
	wxCheckBox* _outline_content;
	wxRadioButton* _left_eye;
	wxRadioButton* _right_eye;
	wxSlider* _slider;
	wxButton* _back_button;
	wxButton* _forward_button;
	wxStaticText* _frame_number;
	wxStaticText* _timecode;
	wxToggleButton* _play_button;
	wxTimer _timer;
	bool _coalesce_player_changes;
	bool _pending_player_change;

	boost::shared_ptr<const Image> _frame;
	DCPTime _position;
	Position<int> _inter_position;
	dcp::Size _inter_size;

	/** Size of our output (including padding if we have any) */
	dcp::Size _out_size;
	/** Size of the panel that we have available */
	dcp::Size _panel_size;
	/** true if the last call to ::get() was specified to be accurate;
	 *  this is used so that when re-fetching the current frame we
	 *  can get the same one that we got last time.
	 */
	bool _last_get_accurate;

	boost::signals2::scoped_connection _film_connection;
	boost::signals2::scoped_connection _player_connection;
};
