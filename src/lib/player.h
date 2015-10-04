/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_PLAYER_H
#define DCPOMATIC_PLAYER_H

#include "player_subtitles.h"
#include "film.h"
#include "content.h"
#include "position_image.h"
#include "piece.h"
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

/** @class Player
 *  @brief A class which can `play' a Playlist.
 */
class Player : public boost::enable_shared_from_this<Player>, public boost::noncopyable
{
public:
	Player (boost::shared_ptr<const Film>, boost::shared_ptr<const Playlist> playlist);

	std::list<boost::shared_ptr<PlayerVideo> > get_video (DCPTime time, bool accurate);
	boost::shared_ptr<AudioBuffers> get_audio (DCPTime time, DCPTime length, bool accurate);
	PlayerSubtitles get_subtitles (DCPTime time, DCPTime length, bool starting, bool burnt);
	std::list<boost::shared_ptr<Font> > get_subtitle_fonts ();
	std::list<boost::shared_ptr<dcp::ReelAsset> > get_reel_assets ();

	void set_video_container_size (dcp::Size);
	void set_ignore_video ();
	void set_ignore_audio ();
	void set_enable_subtitles (bool enable);
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

private:
	friend class PlayerWrapper;
	friend class Piece;
	friend struct player_overlaps_test;
	friend struct player_time_calculation_test1;
	friend struct player_time_calculation_test2;
	friend struct player_time_calculation_test3;

	void setup_pieces ();
	void flush ();
	void film_changed (Film::Property);
	void playlist_changed ();
	void playlist_content_changed (boost::weak_ptr<Content>, int, bool);
	std::list<PositionImage> transform_image_subtitles (std::list<ImageSubtitle>) const;
	void update_subtitle_from_text ();
	Frame dcp_to_content_video (boost::shared_ptr<const Piece> piece, DCPTime t) const;
	DCPTime content_video_to_dcp (boost::shared_ptr<const Piece> piece, Frame f) const;
	Frame dcp_to_resampled_audio (boost::shared_ptr<const Piece> piece, DCPTime t) const;
	ContentTime dcp_to_content_subtitle (boost::shared_ptr<const Piece> piece, DCPTime t) const;
	DCPTime content_subtitle_to_dcp (boost::shared_ptr<const Piece> piece, ContentTime t) const;
	boost::shared_ptr<PlayerVideo> black_player_video_frame (DCPTime) const;

	/** @return Pieces of content type C that overlap a specified time range in the DCP */
	template<class C>
	std::list<boost::shared_ptr<Piece> >
	overlaps (DCPTime from, DCPTime to)
	{
		if (!_have_valid_pieces) {
			setup_pieces ();
		}

		std::list<boost::shared_ptr<Piece> > overlaps;
		for (typename std::list<boost::shared_ptr<Piece> >::const_iterator i = _pieces.begin(); i != _pieces.end(); ++i) {
			if (!boost::dynamic_pointer_cast<C> ((*i)->content)) {
				continue;
			}

			if ((*i)->content->position() <= to && (*i)->content->end() >= from) {
				overlaps.push_back (*i);
			}
		}

		return overlaps;
	}

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
	bool _ignore_audio;
	/** true if the player should always burn subtitles into the video regardless
	    of content settings
	*/
	bool _always_burn_subtitles;
	/** true if we should try to be fast rather than high quality */
	bool _fast;
	/** true if we should `play' (i.e output) referenced DCP data (e.g. for preview) */
	bool _play_referenced;

	boost::shared_ptr<AudioProcessor> _audio_processor;

	boost::signals2::scoped_connection _film_changed_connection;
	boost::signals2::scoped_connection _playlist_changed_connection;
	boost::signals2::scoped_connection _playlist_content_changed_connection;
};

#endif
