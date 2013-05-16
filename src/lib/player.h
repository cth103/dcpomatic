/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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
#include "video_source.h"
#include "audio_source.h"
#include "video_sink.h"
#include "audio_sink.h"
#include "playlist.h"
#include "audio_buffers.h"

class Job;
class Film;
class Playlist;
class AudioContent;
class Decoder;

/** @class Player
 *  @brief A class which can `play' a Playlist; emitting its audio and video.
 */
 
class Player : public VideoSource, public AudioSource, public boost::enable_shared_from_this<Player>
{
public:
	Player (boost::shared_ptr<const Film>, boost::shared_ptr<const Playlist>);

	void disable_video ();
	void disable_audio ();
	void disable_subtitles ();

	bool pass ();
	bool seek (Time);
	void seek_back ();
	void seek_forward ();

	Time last_video () const {
		return _last_video;
	}

private:

	struct RegionDecoder
	{
		RegionDecoder ()
			: last (0)
		{}
		
		Playlist::Region region;
		boost::shared_ptr<Decoder> decoder;
		Time last;
	};
	
	void process_video (boost::shared_ptr<RegionDecoder>, boost::shared_ptr<const Image>, bool, boost::shared_ptr<Subtitle>, Time);
	void process_audio (boost::shared_ptr<RegionDecoder>, boost::shared_ptr<const AudioBuffers>, Time);
	void setup_decoders ();
	void playlist_changed ();
	void content_changed (boost::weak_ptr<Content>, int);
	void emit_black_frame ();
	void emit_silence (Time);

	boost::shared_ptr<const Film> _film;
	boost::shared_ptr<const Playlist> _playlist;
	
	bool _video;
	bool _audio;
	bool _subtitles;

	/** Our decoders are ready to go; if this is false the decoders must be (re-)created before they are used */
	bool _have_valid_decoders;
	std::list<boost::shared_ptr<RegionDecoder> > _decoders;

	Time _position;
	AudioBuffers _audio_buffers;
	Time _last_video;
	bool _last_was_black;
	Time _last_audio;
};

#endif
