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

class VideoDecoder;
class AudioDecoder;
class Job;
class Film;
class Playlist;
class AudioContent;

/** @class Player
 *  @brief A class which can `play' a Playlist; emitting its audio and video.
 */
 
class Player : public VideoSource, public AudioSource, public VideoSink, public boost::enable_shared_from_this<Player>
{
public:
	Player (boost::shared_ptr<const Film>, boost::shared_ptr<const Playlist>);

	void disable_video ();
	void disable_audio ();
	void disable_subtitles ();

	bool pass ();
	bool seek (double);
	void seek_back ();
	void seek_forward ();

	double last_video_time () const;

private:
	void process_video (boost::shared_ptr<const Image> i, bool same, boost::shared_ptr<Subtitle> s, double);
	void process_audio (boost::weak_ptr<const AudioContent>, boost::shared_ptr<const AudioBuffers>, double);
	void setup_decoders ();
	void playlist_changed ();
	void content_changed (boost::weak_ptr<Content>, int);

	boost::shared_ptr<const Film> _film;
	boost::shared_ptr<const Playlist> _playlist;
	
	bool _video;
	bool _audio;
	bool _subtitles;

	/** Our decoders are ready to go; if this is false the decoders must be (re-)created before they are used */
	bool _have_valid_decoders;
	/** Video decoders in order of presentation */
	std::vector<boost::shared_ptr<VideoDecoder> > _video_decoders;
	/** Start positions of each video decoder in seconds*/
	std::vector<double> _video_start;
        /** Index of current video decoder */
	size_t _video_decoder;
        /** Audio decoders in order of presentation (if they are from FFmpeg) */
	std::vector<boost::shared_ptr<AudioDecoder> > _audio_decoders;
	/** Start positions of each audio decoder (if they are from FFmpeg) in seconds */
	std::vector<double> _audio_start;
	/** Current audio decoder index if we are running them sequentially; otherwise undefined */
	size_t _sequential_audio_decoder;

	boost::shared_ptr<AudioBuffers> _audio_buffers;
	boost::optional<double> _audio_time;
};

#endif
