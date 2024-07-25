/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include "video_view.h"
#include "lib/config.h"
#include "lib/film_property.h"
#include "lib/player_text.h"
#include "lib/signaller.h"
#include "lib/timer.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <RtAudio.h>
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <vector>


class Butler;
class ClosedCaptionsDialog;
class FFmpegPlayer;
class Image;
class Player;
class PlayerVideo;
class RGBPlusAlphaImage;
class wxToggleButton;


/** @class FilmViewer
 *  @brief A wx widget to view a Film.
 */
class FilmViewer : public Signaller
{
public:
	FilmViewer (wxWindow *);
	~FilmViewer ();

	/** @return the window showing the film's video */
	wxWindow* panel () const {
		return _video_view->get();
	}

	std::shared_ptr<const VideoView> video_view () const {
		return _video_view;
	}

	void show_closed_captions ();

	void set_film (std::shared_ptr<Film>);
	std::shared_ptr<Film> film () const {
		return _film;
	}

	void seek (dcpomatic::DCPTime t, bool accurate);
	void seek (std::shared_ptr<Content> content, dcpomatic::ContentTime p, bool accurate);
	void seek_by (dcpomatic::DCPTime by, bool accurate);
	/** @return our `playhead' position; this may not lie exactly on a frame boundary */
	dcpomatic::DCPTime position () const {
		return _video_view->position();
	}
	boost::optional<dcpomatic::ContentTime> position_in_content (std::shared_ptr<const Content> content) const;
	dcpomatic::DCPTime one_video_frame () const;

	void start ();
	bool stop ();
	void suspend ();
	void resume ();

	bool playing () const {
		return _playing;
	}

	void set_coalesce_player_changes (bool c);
	void set_dcp_decode_reduction (boost::optional<int> reduction);
	boost::optional<int> dcp_decode_reduction () const;
	void set_outline_content (bool o);
	void set_outline_subtitles (boost::optional<dcpomatic::Rect<double>>);
	void set_eyes (Eyes e);
	void set_pad_black (bool p);
	void set_optimise_for_j2k (bool o);
	void set_crop_guess (dcpomatic::Rect<float> crop);
	void unset_crop_guess ();

	void slow_refresh ();

	dcpomatic::DCPTime time () const;
	boost::optional<dcpomatic::DCPTime> audio_time () const;

	int dropped () const;
	int errored () const;
	int gets () const;

	int audio_callback (void* out, unsigned int frames);

	StateTimer const & state_timer () const {
		return _video_view->state_timer ();
	}

	/* Some accessors and utility methods that VideoView classes need */
	bool outline_content () const {
		return _outline_content;
	}
	boost::optional<dcpomatic::Rect<double>> outline_subtitles () const {
		return _outline_subtitles;
	}
	bool pad_black () const {
		return _pad_black;
	}
	std::shared_ptr<Butler> butler () const {
		return _butler;
	}
	ClosedCaptionsDialog* closed_captions_dialog () const {
		return _closed_captions_dialog;
	}
	void finished ();
	void image_changed (std::shared_ptr<PlayerVideo> video);
	boost::optional<dcpomatic::Rect<float>> crop_guess () const {
		return _crop_guess;
	}

	bool pending_idle_get () const {
		return _idle_get;
	}

	boost::signals2::signal<void (std::shared_ptr<PlayerVideo>)> ImageChanged;
	boost::signals2::signal<void ()> Started;
	boost::signals2::signal<void ()> Stopped;
	/** While playing back we reached the end of the film (emitted from GUI thread) */
	boost::signals2::signal<void ()> Finished;
	/** Emitted from the GUI thread when a lot of frames are being dropped */
	boost::signals2::signal<void()> TooManyDropped;

	boost::signals2::signal<bool ()> PlaybackPermitted;

private:

	void video_view_sized ();
	void calculate_sizes ();
	void player_change (ChangeType type, int, bool);
	void player_change (std::vector<int> properties);
	void idle_handler ();
	void request_idle_display_next_frame ();
	void film_change(ChangeType, FilmProperty);
	void destroy_butler();
	void create_butler();
	void destroy_and_maybe_create_butler();
	void config_changed (Config::Property);
	void film_length_change ();
	void ui_finished ();
	void start_audio_stream_if_open ();

	dcpomatic::DCPTime uncorrected_time () const;
	Frame average_latency () const;

	bool quick_refresh ();

	std::shared_ptr<Film> _film;
	boost::optional<Player> _player;

	std::shared_ptr<VideoView> _video_view;
	bool _coalesce_player_changes = false;
	std::vector<int> _pending_player_changes;

	int _audio_channels = 0;
	unsigned int _audio_block_size = 1024;
	bool _playing = false;
	int _suspended = 0;
	std::shared_ptr<Butler> _butler;

	std::list<Frame> _latency_history;
	/** Mutex to protect _latency_history */
	mutable boost::mutex _latency_history_mutex;
	int _latency_history_count = 0;

	boost::optional<int> _dcp_decode_reduction;

	/** true to assume that this viewer is only being used for JPEG2000 sources
	 *  so it can optimise accordingly.
	 */
	bool _optimise_for_j2k = false;

	ClosedCaptionsDialog* _closed_captions_dialog = nullptr;

	bool _outline_content = false;
	boost::optional<dcpomatic::Rect<double>> _outline_subtitles;
	/** true to pad the viewer panel with black, false to use
	    the normal window background colour.
	*/
	bool _pad_black = false;

	/** true if an get() is required next time we are idle */
	bool _idle_get = false;

	boost::optional<dcpomatic::Rect<float>> _crop_guess;

	boost::signals2::scoped_connection _config_changed_connection;
};
