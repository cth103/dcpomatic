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

class Job;
class Film;
class Playlist;
class AudioContent;
class Piece;
class Image;
class DCPVideo;
class Decoder;

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

/** A wrapper for an Image which contains some pending operations; these may
 *  not be necessary if the receiver of the PlayerImage throws it away.
 */
class PlayerImage
{
public:
	PlayerImage (boost::shared_ptr<const Image>, Crop, dcp::Size, dcp::Size, Scaler const *);

	void set_subtitle (boost::shared_ptr<const Image>, Position<int>);
	
	boost::shared_ptr<Image> image ();
	
private:
	boost::shared_ptr<const Image> _in;
	Crop _crop;
	dcp::Size _inter_size;
	dcp::Size _out_size;
	Scaler const * _scaler;
	boost::shared_ptr<const Image> _subtitle_image;
	Position<int> _subtitle_position;
};

/** @class Player
 *  @brief A class which can `play' a Playlist.
 */
class Player : public boost::enable_shared_from_this<Player>, public boost::noncopyable
{
public:
	Player (boost::shared_ptr<const Film>, boost::shared_ptr<const Playlist>);

	boost::shared_ptr<DCPVideo> get_video (DCPTime time, bool accurate);
	boost::shared_ptr<AudioBuffers> get_audio (DCPTime time, DCPTime length, bool accurate);

	void set_video_container_size (dcp::Size);
	void set_approximate_size ();
	void set_burn_subtitles (bool burn) {
		_burn_subtitles = burn;
	}

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
	friend class player_overlaps_test;

	void setup_pieces ();
	void playlist_changed ();
	void content_changed (boost::weak_ptr<Content>, int, bool);
	void flush ();
	void film_changed (Film::Property);
	std::list<PositionImage> process_content_image_subtitles (
		boost::shared_ptr<SubtitleContent>, std::list<boost::shared_ptr<ContentImageSubtitle> >
		);
	std::list<PositionImage> process_content_text_subtitles (std::list<boost::shared_ptr<ContentTextSubtitle> >);
	void update_subtitle_from_text ();
	VideoFrame dcp_to_content_video (boost::shared_ptr<const Piece> piece, DCPTime t) const;
	AudioFrame dcp_to_content_audio (boost::shared_ptr<const Piece> piece, DCPTime t) const;
	ContentTime dcp_to_content_subtitle (boost::shared_ptr<const Piece> piece, DCPTime t) const;
	boost::shared_ptr<DCPVideo> black_dcp_video (DCPTime) const;

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
