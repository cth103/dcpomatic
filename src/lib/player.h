/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

#include <list>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "playlist.h"
#include "content.h"
#include "film.h"
#include "rect.h"
#include "audio_content.h"
#include "dcpomatic_time.h"
#include "content_subtitle.h"
#include "position_image.h"
#include "piece.h"
#include "content_video.h"
#include "player_subtitles.h"

class Job;
class Film;
class Playlist;
class AudioContent;
class Piece;
class Image;
class Decoder;
class Resampler;
class PlayerVideo;
class ImageProxy;
 
class PlayerStatistics
{
public:
	struct Video {
		Video ()
			: black (0)
			, repeat (0)
			, good (0)
			, skip (0)
		{}
		
		int black;
		int repeat;
		int good;
		int skip;
	} video;

	struct Audio {
		Audio ()
			: silence (0)
			, good (0)
			, skip (0)
		{}
		
		DCPTime silence;
		int64_t good;
		int64_t skip;
	} audio;

	void dump (boost::shared_ptr<Log>) const;
};

/** @class Player
 *  @brief A class which can `play' a Playlist.
 */
class Player : public boost::enable_shared_from_this<Player>, public boost::noncopyable
{
public:
	Player (boost::shared_ptr<const Film>, boost::shared_ptr<const Playlist>);

	std::list<boost::shared_ptr<PlayerVideo> > get_video (DCPTime time, bool accurate);
	boost::shared_ptr<AudioBuffers> get_audio (DCPTime time, DCPTime length, bool accurate);
	PlayerSubtitles get_subtitles (DCPTime time, DCPTime length, bool starting);

	void set_video_container_size (dcp::Size);
	void set_approximate_size ();

	PlayerStatistics const & statistics () const;
	
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

	void setup_pieces ();
	void playlist_changed ();
	void content_changed (boost::weak_ptr<Content>, int, bool);
	void flush ();
	void film_changed (Film::Property);
	std::list<PositionImage> transform_image_subtitles (std::list<ImageSubtitle>) const;
	void update_subtitle_from_text ();
	VideoFrame dcp_to_content_video (boost::shared_ptr<const Piece> piece, DCPTime t) const;
	DCPTime content_video_to_dcp (boost::shared_ptr<const Piece> piece, VideoFrame f) const;
	AudioFrame dcp_to_content_audio (boost::shared_ptr<const Piece> piece, DCPTime t) const;
	ContentTime dcp_to_content_subtitle (boost::shared_ptr<const Piece> piece, DCPTime t) const;
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

	bool _approximate_size;
	bool _burn_subtitles;

	PlayerStatistics _statistics;

	boost::signals2::scoped_connection _playlist_changed_connection;
	boost::signals2::scoped_connection _playlist_content_changed_connection;
	boost::signals2::scoped_connection _film_changed_connection;
};

#endif
