/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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
 *  @brief FilmViewer class.
 */

#include "lib/film.h"
#include "lib/config.h"
#include "lib/player_text.h"
#include <RtAudio.h>
#include <wx/wx.h>

class wxToggleButton;
class FFmpegPlayer;
class Image;
class RGBPlusAlphaImage;
class PlayerVideo;
class Player;
class Butler;
class ClosedCaptionsDialog;

/** @class FilmViewer
 *  @brief A wx widget to view a Film.
 */
class FilmViewer
{
public:
	FilmViewer (wxWindow *);
	~FilmViewer ();

	/** @return the panel showing the film's video */
	wxPanel* panel () const {
		return _panel;
	}

	void show_closed_captions ();

	void set_film (boost::shared_ptr<Film>);
	boost::shared_ptr<Film> film () const {
		return _film;
	}

	void seek (DCPTime t, bool accurate);
	void seek (boost::shared_ptr<Content> content, ContentTime p, bool accurate);
	void seek_by (DCPTime by, bool accurate);
	/** @return our `playhead' position; this may not lie exactly on a frame boundary */
	DCPTime position () const {
		return _video_position;
	}
	DCPTime one_video_frame () const;

	void start ();
	bool stop ();
	bool playing () const {
		return _playing;
	}

	void set_coalesce_player_changes (bool c);
	void set_dcp_decode_reduction (boost::optional<int> reduction);
	boost::optional<int> dcp_decode_reduction () const;
	void set_outline_content (bool o);
	void set_eyes (Eyes e);
	void set_pad_black (bool p);

	void slow_refresh ();

	int dropped () const {
		return _dropped;
	}

	int audio_callback (void* out, unsigned int frames);

#ifdef DCPOMATIC_VARIANT_SWAROOP
	void set_background_image (bool b) {
		_background_image = b;
		refresh_panel ();
	}
#endif

	boost::signals2::signal<void (boost::weak_ptr<PlayerVideo>)> ImageChanged;
	boost::signals2::signal<void ()> PositionChanged;
	boost::signals2::signal<void (DCPTime)> Started;
	boost::signals2::signal<void (DCPTime)> Stopped;
	/** While playing back we reached the end of the film (emitted from GUI thread) */
	boost::signals2::signal<void ()> Finished;

	boost::signals2::signal<bool ()> PlaybackPermitted;

private:
	void paint_panel ();
	void panel_sized (wxSizeEvent &);
	void timer ();
	void calculate_sizes ();
	void player_change (ChangeType type, int, bool);
	void get ();
	void display_player_video ();
	void film_change (ChangeType, Film::Property);
	void content_change (ChangeType, int property);
	void recreate_butler ();
	void config_changed (Config::Property);
	bool maybe_draw_background_image (wxPaintDC& dc);

	DCPTime time () const;
	DCPTime uncorrected_time () const;
	Frame average_latency () const;

	void refresh_panel ();
	bool quick_refresh ();

	boost::shared_ptr<Film> _film;
	boost::shared_ptr<Player> _player;

	/** The area that we put our image in */
	wxPanel* _panel;
	wxTimer _timer;
	bool _coalesce_player_changes;
	std::list<int> _pending_player_changes;

	std::pair<boost::shared_ptr<PlayerVideo>, DCPTime> _player_video;
	boost::shared_ptr<const Image> _frame;
	DCPTime _video_position;
	Position<int> _inter_position;
	dcp::Size _inter_size;

	/** Size of our output (including padding if we have any) */
	dcp::Size _out_size;
	/** Size of the panel that we have available */
	dcp::Size _panel_size;

	RtAudio _audio;
	int _audio_channels;
	unsigned int _audio_block_size;
	bool _playing;
	boost::shared_ptr<Butler> _butler;

	std::list<Frame> _latency_history;
	/** Mutex to protect _latency_history */
	mutable boost::mutex _latency_history_mutex;
	int _latency_history_count;

	int _dropped;
	boost::optional<int> _dcp_decode_reduction;

	ClosedCaptionsDialog* _closed_captions_dialog;

	bool _outline_content;
	Eyes _eyes;
	/** true to pad the viewer panel with black, false to use
	    the normal window background colour.
	*/
	bool _pad_black;

#ifdef DCPOMATIC_VARIANT_SWAROOP
	bool _in_watermark;
	int _watermark_x;
	int _watermark_y;
	bool _background_image;
#endif

	boost::signals2::scoped_connection _config_changed_connection;
};
