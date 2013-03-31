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
#include "lib/decoder_factory.h"

class wxToggleButton;
class FFmpegPlayer;
class Image;
class RGBPlusAlphaImage;
class Subtitle;

/** @class FilmViewer
 *  @brief A wx widget to view a preview of a Film.
 */
class FilmViewer : public wxPanel
{
public:
	FilmViewer (boost::shared_ptr<Film>, wxWindow *);

	void set_film (boost::shared_ptr<Film>);

private:
	void film_changed (Film::Property);
	void paint_panel (wxPaintEvent &);
	void panel_sized (wxSizeEvent &);
	void slider_moved (wxScrollEvent &);
	void play_clicked (wxCommandEvent &);
	void timer (wxTimerEvent &);
	void process_video (boost::shared_ptr<Image>, bool, boost::shared_ptr<Subtitle>);
	void calculate_sizes ();
	void check_play_state ();
	void update_from_raw ();
	void decoder_changed ();
	void raw_to_display ();
	void get_frame ();
	void active_jobs_changed (bool);

	boost::shared_ptr<Film> _film;
	boost::shared_ptr<Playlist> _playlist;

	wxSizer* _v_sizer;
	wxPanel* _panel;
	wxSlider* _slider;
	wxToggleButton* _play_button;
	wxTimer _timer;

	boost::shared_ptr<Image> _raw_frame;
	boost::shared_ptr<Subtitle> _raw_sub;
	boost::shared_ptr<Image> _display_frame;
	int _display_frame_x;
	boost::shared_ptr<RGBPlusAlphaImage> _display_sub;
	Position _display_sub_position;
	bool _got_frame;

	/** Size of our output (including padding if we have any) */
	libdcp::Size _out_size;
	/** Size that we will make our film (equal to _out_size unless we have padding) */
	libdcp::Size _film_size;
	/** Size of the panel that we have available */
	libdcp::Size _panel_size;

	bool _clear_required;
};
