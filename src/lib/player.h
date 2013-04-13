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
class SndfileDecoder;
class Job;
class Film;
class Playlist;
class AudioContent;

class Player : public VideoSource, public AudioSource, public VideoSink, public boost::enable_shared_from_this<Player>
{
public:
	Player (boost::shared_ptr<const Film>, boost::shared_ptr<const Playlist>);

	void disable_video ();
	void disable_audio ();
	void disable_subtitles ();
	void disable_video_sync ();

	bool pass ();
	void set_progress (boost::shared_ptr<Job>);
	bool seek (double);

	double last_video_time () const;

private:
	void process_video (boost::shared_ptr<Image> i, bool same, boost::shared_ptr<Subtitle> s);
	void process_audio (boost::weak_ptr<const AudioContent>, boost::shared_ptr<AudioBuffers>);
	void setup_decoders ();
	void playlist_changed ();
	void content_changed (boost::weak_ptr<Content>, int);

	boost::shared_ptr<const Film> _film;
	boost::shared_ptr<const Playlist> _playlist;
	
	bool _video;
	bool _audio;
	bool _subtitles;
	
	bool _have_valid_decoders;
	std::list<boost::shared_ptr<VideoDecoder> > _video_decoders;
	std::list<boost::shared_ptr<VideoDecoder> >::iterator _video_decoder;
	std::list<boost::shared_ptr<SndfileDecoder> > _sndfile_decoders;

	boost::shared_ptr<AudioBuffers> _audio_buffers;

	bool _video_sync;
};

#endif
