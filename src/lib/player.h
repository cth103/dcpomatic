/*
    Copyright (C) 2013-2020 Carl Hetherington <cth@carlh.net>

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


#include "active_text.h"
#include "atmos_metadata.h"
#include "audio_merger.h"
#include "audio_stream.h"
#include "content_atmos.h"
#include "content_audio.h"
#include "content_text.h"
#include "content_video.h"
#include "dcp_text_track.h"
#include "empty.h"
#include "enum_indexed_vector.h"
#include "film_property.h"
#include "image.h"
#include "player_text.h"
#include "position_image.h"
#include "shuffler.h"
#include <boost/atomic.hpp>
#include <list>


namespace dcp {
	class ReelAsset;
}

class AtmosContent;
class AudioBuffers;
class Content;
class Film;
class PlayerVideo;
class Playlist;
class ReferencedReelAsset;


class PlayerProperty
{
public:
	static int const VIDEO_CONTAINER_SIZE;
	static int const PLAYLIST;
	static int const FILM_CONTAINER;
	static int const FILM_VIDEO_FRAME_RATE;
	static int const DCP_DECODE_REDUCTION;
	static int const PLAYBACK_LENGTH;
	static int const IGNORE_VIDEO;
	static int const IGNORE_AUDIO;
	static int const IGNORE_TEXT;
	static int const ALWAYS_BURN_OPEN_SUBTITLES;
	static int const PLAY_REFERENCED;
};


/** @class Player
 *  @brief A class which can play a Playlist.
 */
class Player
{
public:
	Player (std::shared_ptr<const Film>, Image::Alignment subtitle_alignment);
	Player (std::shared_ptr<const Film>, std::shared_ptr<const Playlist> playlist);

	Player (Player const&) = delete;
	Player& operator= (Player const&) = delete;

	Player(Player&& other);
	Player& operator=(Player&& other);

	bool pass ();
	void seek (dcpomatic::DCPTime time, bool accurate);
	Frame frames_done() const;
	float progress() const;

	std::vector<std::shared_ptr<dcpomatic::Font>> get_subtitle_fonts ();

	dcp::Size video_container_size () const {
		return _video_container_size;
	}

	void set_video_container_size (dcp::Size);
	void set_ignore_video ();
	void set_ignore_audio ();
	void set_ignore_text ();
	void set_always_burn_open_subtitles ();
	void set_fast ();
	void set_play_referenced ();
	void set_dcp_decode_reduction (boost::optional<int> reduction);
	void set_disable_audio_processor();

	boost::optional<dcpomatic::DCPTime> content_time_to_dcp (std::shared_ptr<const Content> content, dcpomatic::ContentTime t) const;
	boost::optional<dcpomatic::ContentTime> dcp_to_content_time (std::shared_ptr<const Content> content, dcpomatic::DCPTime t) const;

	void signal_change(ChangeType type, int property);

	/** First parameter is PENDING, DONE or CANCELLED.
	 *  Second parameter is the property.
	 *  Third parameter is true if these signals are currently likely to be frequent.
	 */
	boost::signals2::signal<void (ChangeType, int, bool)> Change;

	/** Emitted when a video frame is ready.  These emissions happen in the correct order. */
	boost::signals2::signal<void (std::shared_ptr<PlayerVideo>, dcpomatic::DCPTime)> Video;
	/** Emitted when audio data is ready.  First parameter is the audio data, second its time,
	 *  third the frame rate.
	 */
	boost::signals2::signal<void (std::shared_ptr<AudioBuffers>, dcpomatic::DCPTime, int)> Audio;
	/** Emitted when a text is ready.  This signal may be emitted considerably
	 *  after the corresponding Video.
	 */
	boost::signals2::signal<void (PlayerText, TextType, boost::optional<DCPTextTrack>, dcpomatic::DCPTimePeriod)> Text;
	boost::signals2::signal<void (std::shared_ptr<const dcp::AtmosFrame>, dcpomatic::DCPTime, AtmosMetadata)> Atmos;

private:
	friend class PlayerWrapper;
	friend class Piece;
	friend struct player_time_calculation_test1;
	friend struct player_time_calculation_test2;
	friend struct player_time_calculation_test3;
	friend struct player_subframe_test;
	friend struct empty_test1;
	friend struct empty_test2;
	friend struct check_reuse_old_data_test;
	friend struct overlap_video_test1;

	void construct ();
	void connect();
	void setup_pieces ();
	void film_change(ChangeType, FilmProperty);
	void playlist_change (ChangeType);
	void playlist_content_change (ChangeType, int, bool);
	Frame dcp_to_content_video (std::shared_ptr<const Piece> piece, dcpomatic::DCPTime t) const;
	dcpomatic::DCPTime content_video_to_dcp (std::shared_ptr<const Piece> piece, Frame f) const;
	Frame dcp_to_resampled_audio (std::shared_ptr<const Piece> piece, dcpomatic::DCPTime t) const;
	dcpomatic::DCPTime resampled_audio_to_dcp (std::shared_ptr<const Piece> piece, Frame f) const;
	dcpomatic::ContentTime dcp_to_content_time (std::shared_ptr<const Piece> piece, dcpomatic::DCPTime t) const;
	dcpomatic::DCPTime content_time_to_dcp (std::shared_ptr<const Piece> piece, dcpomatic::ContentTime t) const;
	std::shared_ptr<PlayerVideo> black_player_video_frame (Eyes eyes) const;
	void emit_video_until(dcpomatic::DCPTime time);
	void insert_video(std::shared_ptr<PlayerVideo> pv, dcpomatic::DCPTime time, dcpomatic::DCPTime end);
	std::pair<std::shared_ptr<Piece>, boost::optional<dcpomatic::DCPTime>> earliest_piece_and_time() const;

	void video (std::weak_ptr<Piece>, ContentVideo);
	void audio (std::weak_ptr<Piece>, AudioStreamPtr, ContentAudio);
	void bitmap_text_start (std::weak_ptr<Piece>, std::weak_ptr<const TextContent>, ContentBitmapText);
	void plain_text_start (std::weak_ptr<Piece>, std::weak_ptr<const TextContent>, ContentStringText);
	void subtitle_stop (std::weak_ptr<Piece>, std::weak_ptr<const TextContent>, dcpomatic::ContentTime);
	void atmos (std::weak_ptr<Piece>, ContentAtmos);

	dcpomatic::DCPTime one_video_frame () const;
	void fill_audio (dcpomatic::DCPTimePeriod period);
	std::pair<std::shared_ptr<AudioBuffers>, dcpomatic::DCPTime> discard_audio (
		std::shared_ptr<const AudioBuffers> audio, dcpomatic::DCPTime time, dcpomatic::DCPTime discard_to
		) const;
	boost::optional<PositionImage> open_subtitles_for_frame (dcpomatic::DCPTime time) const;
	void emit_video(std::shared_ptr<PlayerVideo> pv, dcpomatic::DCPTime time);
	void use_video(std::shared_ptr<PlayerVideo> pv, dcpomatic::DCPTime time, dcpomatic::DCPTime end);
	void emit_audio (std::shared_ptr<AudioBuffers> data, dcpomatic::DCPTime time);
	std::shared_ptr<const Playlist> playlist () const;

	/** Mutex to protect the most of the Player state.  When it's used for the preview we have
	    seek() and pass() called from the Butler thread and lots of other stuff called
	    from the GUI thread.
	*/
	mutable boost::mutex _mutex;

	std::weak_ptr<const Film> _film;
	/** Playlist, or 0 if we are using the one from the _film */
	std::shared_ptr<const Playlist> _playlist;

	/** > 0 if we are suspended (i.e. pass() and seek() do nothing) */
	boost::atomic<int> _suspended;
	std::vector<std::shared_ptr<Piece>> _pieces;

	/** Size of the image we are rendering to; this may be the DCP frame size, or
	 *  the size of preview in a window.
	 */
	boost::atomic<dcp::Size> _video_container_size;

	mutable boost::mutex _black_image_mutex;
	std::shared_ptr<Image> _black_image;

	/** true if the player should ignore all video; i.e. never produce any */
	boost::atomic<bool> _ignore_video;
	boost::atomic<bool> _ignore_audio;
	/** true if the player should ignore all text; i.e. never produce any */
	boost::atomic<bool> _ignore_text;
	boost::atomic<bool> _always_burn_open_subtitles;
	/** true if we should try to be fast rather than high quality */
	boost::atomic<bool> _fast;
	/** true if we should keep going in the face of `survivable' errors */
	bool _tolerant;
	/** true if we should `play' (i.e output) referenced DCP data (e.g. for preview) */
	boost::atomic<bool> _play_referenced;

	/** Time of the next video that we will emit, or the time of the last accurate seek */
	boost::optional<dcpomatic::DCPTime> _next_video_time;
	/** Time of the next audio that we will emit, or the time of the last accurate seek */
	boost::optional<dcpomatic::DCPTime> _next_audio_time;

	boost::atomic<boost::optional<int>> _dcp_decode_reduction;

	EnumIndexedVector<std::pair<std::shared_ptr<PlayerVideo>, dcpomatic::DCPTime>, Eyes> _last_video;

	AudioMerger _audio_merger;
	std::unique_ptr<Shuffler> _shuffler;
	std::list<std::pair<std::shared_ptr<PlayerVideo>, dcpomatic::DCPTime>> _delay;

	class StreamState
	{
	public:
		StreamState () {}

		explicit StreamState(std::shared_ptr<Piece> p)
			: piece(p)
		{}

		std::shared_ptr<Piece> piece;
		boost::optional<dcpomatic::DCPTime> last_push_end;
	};
	std::map<AudioStreamPtr, StreamState> _stream_states;

	Empty _black;
	Empty _silent;

	EnumIndexedVector<ActiveText, TextType> _active_texts;
	std::shared_ptr<AudioProcessor> _audio_processor;
	bool _disable_audio_processor = false;

	boost::atomic<dcpomatic::DCPTime> _playback_length;

	/** Alignment for subtitle images that we create */
	Image::Alignment _subtitle_alignment = Image::Alignment::PADDED;

	boost::signals2::scoped_connection _film_changed_connection;
	boost::signals2::scoped_connection _playlist_change_connection;
	boost::signals2::scoped_connection _playlist_content_change_connection;
};


#endif
