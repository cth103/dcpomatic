/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_PLAYER_H
#define DCPOMATIC_PLAYER_H

#include "player_subtitles.h"
#include "active_subtitles.h"
#include "film.h"
#include "content.h"
#include "position_image.h"
#include "piece.h"
#include "content_video.h"
#include "content_audio.h"
#include "content_subtitle.h"
#include "audio_stream.h"
#include "audio_merger.h"
#include "empty.h"
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <list>

namespace dcp {
	class ReelAsset;
}

class PlayerVideo;
class Playlist;
class Font;
class AudioBuffers;
class ReferencedReelAsset;

/** @class Player
 *  @brief A class which can `play' a Playlist.
 */
class Player : public boost::enable_shared_from_this<Player>, public boost::noncopyable
{
public:
	Player (boost::shared_ptr<const Film>, boost::shared_ptr<const Playlist> playlist);

	bool pass ();
	void seek (DCPTime time, bool accurate);

	std::list<boost::shared_ptr<Font> > get_subtitle_fonts ();
	std::list<ReferencedReelAsset> get_reel_assets ();

	void set_video_container_size (dcp::Size);
	void set_ignore_video ();
	void set_ignore_subtitle ();
	void set_always_burn_subtitles (bool burn);
	void set_fast ();
	void set_play_referenced ();

	/** Emitted when something has changed such that if we went back and emitted
	 *  the last frame again it would look different.  This is not emitted after
	 *  a seek.
	 *
	 *  The parameter is true if these signals are currently likely to be frequent.
	 */
	boost::signals2::signal<void (bool)> Changed;

	boost::signals2::signal<void (boost::shared_ptr<PlayerVideo>, DCPTime)> Video;
	boost::signals2::signal<void (boost::shared_ptr<AudioBuffers>, DCPTime)> Audio;
	/** Emitted when a subtitle is ready.  This signal may be emitted considerably
	 *  after the corresponding Video.
	 */
	boost::signals2::signal<void (PlayerSubtitles, DCPTimePeriod)> Subtitle;

private:
	friend class PlayerWrapper;
	friend class Piece;
	friend struct player_time_calculation_test1;
	friend struct player_time_calculation_test2;
	friend struct player_time_calculation_test3;
	friend struct player_subframe_test;

	void setup_pieces ();
	void flush ();
	void film_changed (Film::Property);
	void playlist_changed ();
	void playlist_content_changed (boost::weak_ptr<Content>, int, bool);
	std::list<PositionImage> transform_image_subtitles (std::list<ImageSubtitle>) const;
	Frame dcp_to_content_video (boost::shared_ptr<const Piece> piece, DCPTime t) const;
	DCPTime content_video_to_dcp (boost::shared_ptr<const Piece> piece, Frame f) const;
	Frame dcp_to_resampled_audio (boost::shared_ptr<const Piece> piece, DCPTime t) const;
	DCPTime resampled_audio_to_dcp (boost::shared_ptr<const Piece> piece, Frame f) const;
	ContentTime dcp_to_content_time (boost::shared_ptr<const Piece> piece, DCPTime t) const;
	DCPTime content_time_to_dcp (boost::shared_ptr<const Piece> piece, ContentTime t) const;
	boost::shared_ptr<PlayerVideo> black_player_video_frame () const;
	void video (boost::weak_ptr<Piece>, ContentVideo);
	void audio (boost::weak_ptr<Piece>, AudioStreamPtr, ContentAudio);
	void image_subtitle_start (boost::weak_ptr<Piece>, ContentImageSubtitle);
	void text_subtitle_start (boost::weak_ptr<Piece>, ContentTextSubtitle);
	void subtitle_stop (boost::weak_ptr<Piece>, ContentTime);
	DCPTime one_video_frame () const;
	void fill_audio (DCPTimePeriod period);
	std::pair<boost::shared_ptr<AudioBuffers>, DCPTime> discard_audio (
		boost::shared_ptr<const AudioBuffers> audio, DCPTime time, DCPTime discard_to
		) const;
	boost::optional<PositionImage> subtitles_for_frame (DCPTime time) const;
	void emit_video (boost::shared_ptr<PlayerVideo> pv, DCPTime time);
	void emit_audio (boost::shared_ptr<AudioBuffers> data, DCPTime time);

	boost::shared_ptr<const Film> _film;
	boost::shared_ptr<const Playlist> _playlist;

	/** Our pieces are ready to go; if this is false the pieces must be (re-)created before they are used */
	bool _have_valid_pieces;
	std::list<boost::shared_ptr<Piece> > _pieces;

	/** Size of the image in the DCP (e.g. 1990x1080 for flat) */
	dcp::Size _video_container_size;
	boost::shared_ptr<Image> _black_image;

	/** true if the player should ignore all video; i.e. never produce any */
	bool _ignore_video;
	/** true if the player should ignore all audio; i.e. never produce any */
	bool _ignore_subtitle;
	/** true if the player should always burn subtitles into the video regardless
	    of content settings
	*/
	bool _always_burn_subtitles;
	/** true if we should try to be fast rather than high quality */
	bool _fast;
	/** true if we should `play' (i.e output) referenced DCP data (e.g. for preview) */
	bool _play_referenced;

	/** Time just after the last video frame we emitted, or the time of the last accurate seek */
	boost::optional<DCPTime> _last_video_time;
	/** Time just after the last audio frame we emitted, or the time of the last accurate seek */
	boost::optional<DCPTime> _last_audio_time;

	typedef std::map<boost::weak_ptr<Piece>, boost::shared_ptr<PlayerVideo> > LastVideoMap;
	LastVideoMap _last_video;

	AudioMerger _audio_merger;

	class StreamState
	{
	public:
		StreamState () {}

		StreamState (boost::shared_ptr<Piece> p, DCPTime l)
			: piece(p)
			, last_push_end(l)
		{}

		boost::shared_ptr<Piece> piece;
		DCPTime last_push_end;
	};
	std::map<AudioStreamPtr, StreamState> _stream_states;

	Empty _black;
	Empty _silent;

	ActiveSubtitles _active_subtitles;
	boost::shared_ptr<AudioProcessor> _audio_processor;

	boost::signals2::scoped_connection _film_changed_connection;
	boost::signals2::scoped_connection _playlist_changed_connection;
	boost::signals2::scoped_connection _playlist_content_changed_connection;
};

#endif
