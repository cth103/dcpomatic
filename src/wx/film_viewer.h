/*
    Copyright (C) 2012-2019 Carl Hetherington <cth@carlh.net>

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
#include "lib/film.h"
#include "lib/config.h"
#include "lib/player_text.h"
#include "lib/timer.h"
#include "lib/signaller.h"
#include "lib/warnings.h"
#include <RtAudio.h>
DCPOMATIC_DISABLE_WARNINGS
#include <wx/wx.h>
DCPOMATIC_ENABLE_WARNINGS

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
class FilmViewer : public Signaller
{
public:
	FilmViewer (wxWindow *);
	~FilmViewer ();

	/** @return the window showing the film's video */
	wxWindow* panel () const {
		return _video_view->get();
	}

	VideoView const * video_view () const {
		return _video_view;
	}

	void show_closed_captions ();

	void set_film (boost::shared_ptr<Film>);
	boost::shared_ptr<Film> film () const {
		return _film;
	}

	void seek (dcpomatic::DCPTime t, bool accurate);
	void seek (boost::shared_ptr<Content> content, dcpomatic::ContentTime p, bool accurate);
	void seek_by (dcpomatic::DCPTime by, bool accurate);
	/** @return our `playhead' position; this may not lie exactly on a frame boundary */
	dcpomatic::DCPTime position () const {
		return _video_view->position();
	}
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
	void set_outline_subtitles (boost::optional<dcpomatic::Rect<double> >);
	void set_eyes (Eyes e);
	void set_pad_black (bool p);

	void slow_refresh ();

	dcpomatic::DCPTime time () const;
	boost::optional<dcpomatic::DCPTime> audio_time () const;

	int dropped () const;
	int errored () const;
	int gets () const;

	int audio_callback (void* out, unsigned int frames);

#ifdef DCPOMATIC_VARIANT_SWAROOP
	void set_background_image (bool b) {
		_background_image = b;
		_video_view->update ();
	}

	bool background_image () const {
		return _background_image;
	}
#endif

	StateTimer const & state_timer () const {
		return _video_view->state_timer ();
	}

	/* Some accessors and utility methods that VideoView classes need */
	dcp::Size out_size () const {
		return _out_size;
	}
	bool outline_content () const {
		return _outline_content;
	}
	boost::optional<dcpomatic::Rect<double> > outline_subtitles () const {
		return _outline_subtitles;
	}
	bool pad_black () const {
		return _pad_black;
	}
	boost::shared_ptr<Butler> butler () const {
		return _butler;
	}
	ClosedCaptionsDialog* closed_captions_dialog () const {
		return _closed_captions_dialog;
	}
	void finished ();

	bool pending_idle_get () const {
		return _idle_get;
	}

	boost::signals2::signal<void (boost::weak_ptr<PlayerVideo>)> ImageChanged;
	boost::signals2::signal<void (dcpomatic::DCPTime)> Started;
	boost::signals2::signal<void (dcpomatic::DCPTime)> Stopped;
	/** While playing back we reached the end of the film (emitted from GUI thread) */
	boost::signals2::signal<void ()> Finished;

	boost::signals2::signal<bool ()> PlaybackPermitted;

private:

	void video_view_sized ();
	void calculate_sizes ();
	void player_change (ChangeType type, int, bool);
	void idle_handler ();
	void request_idle_display_next_frame ();
	void film_change (ChangeType, Film::Property);
	void content_change (ChangeType, int property);
	void recreate_butler ();
	void config_changed (Config::Property);
	void film_length_change ();
	void ui_finished ();

	dcpomatic::DCPTime uncorrected_time () const;
	Frame average_latency () const;

	bool quick_refresh ();

	boost::shared_ptr<Film> _film;
	boost::shared_ptr<Player> _player;

	VideoView* _video_view;
	bool _coalesce_player_changes;
	std::list<int> _pending_player_changes;

	/** Size of our output (including padding if we have any) */
	dcp::Size _out_size;

	RtAudio _audio;
	int _audio_channels;
	unsigned int _audio_block_size;
	bool _playing;
	int _suspended;
	boost::shared_ptr<Butler> _butler;

	std::list<Frame> _latency_history;
	/** Mutex to protect _latency_history */
	mutable boost::mutex _latency_history_mutex;
	int _latency_history_count;

	boost::optional<int> _dcp_decode_reduction;

	ClosedCaptionsDialog* _closed_captions_dialog;

	bool _outline_content;
	boost::optional<dcpomatic::Rect<double> > _outline_subtitles;
	/** true to pad the viewer panel with black, false to use
	    the normal window background colour.
	*/
	bool _pad_black;

#ifdef DCPOMATIC_VARIANT_SWAROOP
	bool _background_image;
#endif

	/** true if an get() is required next time we are idle */
	bool _idle_get;

	boost::signals2::scoped_connection _config_changed_connection;
};
