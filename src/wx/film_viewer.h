/*
    Copyright (C) 2012-2017 Carl Hetherington <cth@carlh.net>

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

#include "lib/film.h"
#include "lib/config.h"
#include <RtAudio.h>
#include <wx/wx.h>

class wxToggleButton;
class FFmpegPlayer;
class Image;
class RGBPlusAlphaImage;
class PlayerVideo;
class Player;
class Butler;

/** @class FilmViewer
 *  @brief A wx widget to view a preview of a Film.
 */
class FilmViewer : public wxPanel
{
public:
	FilmViewer (wxWindow *, bool outline_content = true, bool jump_to_selected = true);
	~FilmViewer ();

	void set_film (boost::shared_ptr<Film>);
	boost::shared_ptr<Film> film () const {
		return _film;
	}

	/** @return our `playhead' position; this may not lie exactly on a frame boundary */
	DCPTime position () const {
		return _video_position;
	}

	void set_position (DCPTime p);
	void set_coalesce_player_changes (bool c);

	void refresh ();

	int audio_callback (void* out, unsigned int frames);

	boost::signals2::signal<void (boost::weak_ptr<PlayerVideo>)> ImageChanged;

private:
	void paint_panel ();
	void panel_sized (wxSizeEvent &);
	void slider_moved (bool update_slider);
	void play_clicked ();
	void timer ();
	void calculate_sizes ();
	void check_play_state ();
	void active_jobs_changed (boost::optional<std::string>);
	void back_clicked (wxMouseEvent &);
	void forward_clicked (wxMouseEvent &);
	void player_changed (bool);
	void update_position_label ();
	void update_position_slider ();
	void get ();
	void seek (DCPTime t, bool accurate);
	void refresh_panel ();
	void setup_sensitivity ();
	void film_changed (Film::Property);
	DCPTime nudge_amount (wxMouseEvent &);
	void timecode_clicked ();
	void frame_number_clicked ();
	void go_to (DCPTime t);
	void jump_to_selected_clicked ();
	void recreate_butler ();
	void config_changed (Config::Property);
	DCPTime time () const;
	void start ();
	bool stop ();
	Frame average_latency () const;

	boost::shared_ptr<Film> _film;
	boost::shared_ptr<Player> _player;

	wxSizer* _v_sizer;
	wxPanel* _panel;
	wxCheckBox* _outline_content;
	wxRadioButton* _left_eye;
	wxRadioButton* _right_eye;
	wxCheckBox* _jump_to_selected;
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
	DCPTime _video_position;
	Position<int> _inter_position;
	dcp::Size _inter_size;

	/** Size of our output (including padding if we have any) */
	dcp::Size _out_size;
	/** Size of the panel that we have available */
	dcp::Size _panel_size;
	/** true if the last call to Player::seek() was specified to be accurate;
	 *  this is used so that when re-fetching the current frame we
	 *  can get the same one that we got last time.
	 */
	bool _last_seek_accurate;

	RtAudio _audio;
	int _audio_channels;
	unsigned int _audio_block_size;
	bool _playing;
	boost::shared_ptr<Butler> _butler;

        std::list<Frame> _latency_history;
        /** Mutex to protect _latency_history */
        mutable boost::mutex _latency_history_mutex;
        int _latency_history_count;

	boost::signals2::scoped_connection _config_changed_connection;
};
