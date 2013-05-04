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

/** @class Playlist
 *  @brief A set of content files (video and audio), with knowledge of how they should be arranged into
 *  a DCP.
 *
 * This class holds Content objects, and it knows how they should be arranged.  At the moment
 * the ordering is implicit; video content is placed sequentially, and audio content is taken
 * from the video unless any sound-only files are present.  If sound-only files exist, they
 * are played simultaneously (i.e. they can be split up into multiple files for different channels)
 */
    
class Playlist
{
public:
	Playlist ();
	Playlist (boost::shared_ptr<const Playlist>);

	void as_xml (xmlpp::Node *);
	void set_from_xml (boost::shared_ptr<const cxml::Node>);

	void add (boost::shared_ptr<Content>);
	void remove (boost::shared_ptr<Content>);
	void move_earlier (boost::shared_ptr<Content>);
	void move_later (boost::shared_ptr<Content>);

	ContentAudioFrame audio_length () const;
	int audio_channels () const;
	int audio_frame_rate () const;
	bool has_audio () const;
	
	float video_frame_rate () const;
	libdcp::Size video_size () const;
	ContentVideoFrame video_length () const;

	AudioMapping default_audio_mapping () const;
	ContentVideoFrame content_length () const;

	enum AudioFrom {
		AUDIO_FFMPEG,
		AUDIO_SNDFILE
	};

	AudioFrom audio_from () const {
		return _audio_from;
	}

	bool has_subtitles () const;
	
	ContentList content () const {
		return _content;
	}

	std::string description () const;

	boost::shared_ptr<FFmpegContent> ffmpeg () const;

	std::list<boost::shared_ptr<const VideoContent> > video () const {
		return _video;
	}

	std::list<boost::shared_ptr<const AudioContent> > audio () const {
		return _audio;
	}

	std::string audio_digest () const;
	std::string video_digest () const;

	int loop () const {
		return _loop;
	}
	
	void set_loop (int l);

	mutable boost::signals2::signal<void ()> Changed;
	mutable boost::signals2::signal<void (boost::weak_ptr<Content>, int)> ContentChanged;
	
private:
	void setup ();
	void content_changed (boost::weak_ptr<Content>, int);

	/** where we should get our audio from */
	AudioFrom _audio_from;

	/** all our content */
	ContentList _content;
	/** all our content which contains video */
	std::list<boost::shared_ptr<const VideoContent> > _video;
	/** all our content which contains audio.  This may contain the same objects
	 *  as _video for FFmpegContent.
	 */
	std::list<boost::shared_ptr<const AudioContent> > _audio;

	int _loop;

	std::list<boost::signals2::connection> _content_connections;
};
