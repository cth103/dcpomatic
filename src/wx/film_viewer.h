/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

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
	FilmViewer (boost::shared_ptr<Film>, wxWindow *);

	void set_film (boost::shared_ptr<Film>);

private:
	void paint_panel ();
	void panel_sized (wxSizeEvent &);
	void slider_moved ();
	void play_clicked ();
	void timer ();
	void calculate_sizes ();
	void check_play_state ();
	void active_jobs_changed (bool);
	void back_clicked ();
	void forward_clicked ();
	void player_changed (bool);
	void set_position_text ();
	void get (DCPTime, bool);
	void refresh_panel ();

	boost::shared_ptr<Film> _film;
	boost::shared_ptr<Player> _player;

	wxSizer* _v_sizer;
	wxPanel* _panel;
	wxCheckBox* _outline_content;
	wxSlider* _slider;
	wxButton* _back_button;
	wxButton* _forward_button;
	wxStaticText* _frame_number;
	wxStaticText* _timecode;
	wxToggleButton* _play_button;
	wxTimer _timer;

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
};
