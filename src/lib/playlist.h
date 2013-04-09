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

#include <list>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "video_source.h"
#include "audio_source.h"
#include "video_sink.h"
#include "audio_sink.h"
#include "ffmpeg_content.h"
#include "audio_mapping.h"

class Content;
class FFmpegContent;
class FFmpegDecoder;
class ImageMagickContent;
class ImageMagickDecoder;
class SndfileContent;
class SndfileDecoder;
class Job;
class Film;

class Playlist
{
public:
	Playlist ();

	void setup (ContentList);

	ContentAudioFrame audio_length () const;
	int audio_channels () const;
	int audio_frame_rate () const;
	int64_t audio_channel_layout () const;
	bool has_audio () const;
	
	float video_frame_rate () const;
	libdcp::Size video_size () const;
	ContentVideoFrame video_length () const;

	AudioMapping default_audio_mapping () const;

	enum AudioFrom {
		AUDIO_FFMPEG,
		AUDIO_SNDFILE
	};

	AudioFrom audio_from () const {
		return _audio_from;
	}

	std::list<boost::shared_ptr<const VideoContent> > video () const {
		return _video;
	}

	std::list<boost::shared_ptr<const SndfileContent> > sndfile () const {
		return _sndfile;
	}

	mutable boost::signals2::signal<void ()> Changed;
	mutable boost::signals2::signal<void (boost::weak_ptr<Content>, int)> ContentChanged;
	
private:
	void content_changed (boost::weak_ptr<Content>, int);
	boost::shared_ptr<const FFmpegContent> first_ffmpeg () const;
	
	AudioFrom _audio_from;

	std::list<boost::shared_ptr<const VideoContent> > _video;
	std::list<boost::shared_ptr<const SndfileContent> > _sndfile;

	std::list<boost::signals2::connection> _content_connections;
};
