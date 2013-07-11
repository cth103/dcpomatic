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
#include "playlist.h"
#include "audio_buffers.h"
#include "content.h"
#include "film.h"
#include "rect.h"

class Job;
class Film;
class Playlist;
class AudioContent;
class Piece;
class Image;
class Resampler;

/** @class Player
 *  @brief A class which can `play' a Playlist; emitting its audio and video.
 */
 
class Player : public boost::enable_shared_from_this<Player>
{
public:
	Player (boost::shared_ptr<const Film>, boost::shared_ptr<const Playlist>);

	void disable_video ();
	void disable_audio ();

	bool pass ();
	void seek (Time, bool);

	Time video_position () const {
		return _video_position;
	}

	void set_video_container_size (libdcp::Size);

	/** Emitted when a video frame is ready.
	 *  First parameter is the video image.
	 *  Second parameter is true if the image is the same as the last one that was emitted.
	 *  Third parameter is the time.
	 */
	boost::signals2::signal<void (boost::shared_ptr<const Image>, bool, Time)> Video;
	
	/** Emitted when some audio data is ready */
	boost::signals2::signal<void (boost::shared_ptr<const AudioBuffers>, Time)> Audio;

	/** Emitted when something has changed such that if we went back and emitted
	 *  the last frame again it would look different.  This is not emitted after
	 *  a seek.
	 */
	boost::signals2::signal<void ()> Changed;

private:

	void process_video (boost::weak_ptr<Piece>, boost::shared_ptr<const Image>, bool, VideoContent::Frame);
	void process_audio (boost::weak_ptr<Piece>, boost::shared_ptr<const AudioBuffers>, AudioContent::Frame);
	void process_subtitle (boost::weak_ptr<Piece>, boost::shared_ptr<Image>, dcpomatic::Rect<double>, Time, Time);
	void setup_pieces ();
	void playlist_changed ();
	void content_changed (boost::weak_ptr<Content>, int);
	void do_seek (Time, bool);
	void flush ();
	void emit_black ();
	void emit_silence (OutputAudioFrame);
	boost::shared_ptr<Resampler> resampler (boost::shared_ptr<AudioContent>);
	void film_changed (Film::Property);
	void update_subtitle ();

	boost::shared_ptr<const Film> _film;
	boost::shared_ptr<const Playlist> _playlist;
	
	bool _video;
	bool _audio;

	/** Our pieces are ready to go; if this is false the pieces must be (re-)created before they are used */
	bool _have_valid_pieces;
	std::list<boost::shared_ptr<Piece> > _pieces;

	/** The time after the last video that we emitted */
	Time _video_position;
	/** The time after the last audio that we emitted */
	Time _audio_position;

	AudioBuffers _audio_buffers;

	libdcp::Size _video_container_size;
	boost::shared_ptr<Image> _black_frame;
	std::map<boost::shared_ptr<AudioContent>, boost::shared_ptr<Resampler> > _resamplers;

	struct {
		boost::weak_ptr<Piece> piece;
		boost::shared_ptr<Image> image;
		dcpomatic::Rect<double> rect;
		Time from;
		Time to;
	} _in_subtitle;

	struct {
		boost::shared_ptr<Image> image;
		Position<int> position;
		Time from;
		Time to;
	} _out_subtitle;
};

#endif
