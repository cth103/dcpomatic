/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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
class PlayerImage;

/** @class FilmViewer
 *  @brief A wx widget to view a preview of a Film.
 *
 *  The film takes the following path through the viewer:
 *
 *  1.	fetch_next_frame() asks our _player to decode some data.  If it does, process_video()
 *	will be called.
 *
 *  2.	process_video() takes the image from the player (_frame).
 *
 *  3.	fetch_next_frame() calls _panel->Refresh() and _panel->Update() which results in
 *	paint_panel() being called; this creates frame_bitmap from _frame and blits it to the display.
 *
 * fetch_current_frame_again() asks the player to re-emit its current frame on the next pass(), and then
 * starts from step #1.
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
	void process_video (boost::shared_ptr<PlayerImage>, Eyes, DCPTime);
	void calculate_sizes ();
	void check_play_state ();
	void fetch_current_frame_again ();
	void fetch_next_frame ();
	void active_jobs_changed (bool);
	void back_clicked ();
	void forward_clicked ();
	void player_changed (bool);
	void set_position_text (DCPTime);

	boost::shared_ptr<Film> _film;
	boost::shared_ptr<Player> _player;

	wxSizer* _v_sizer;
	wxPanel* _panel;
	wxSlider* _slider;
	wxButton* _back_button;
	wxButton* _forward_button;
	wxStaticText* _frame_number;
	wxStaticText* _timecode;
	wxToggleButton* _play_button;
	wxTimer _timer;

	boost::shared_ptr<const Image> _frame;
	bool _got_frame;

	/** Size of our output (including padding if we have any) */
	libdcp::Size _out_size;
	/** Size of the panel that we have available */
	libdcp::Size _panel_size;
};
