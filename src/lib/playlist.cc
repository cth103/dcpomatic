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

#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include "playlist.h"
#include "sndfile_content.h"
#include "sndfile_decoder.h"
#include "video_content.h"
#include "ffmpeg_decoder.h"
#include "ffmpeg_content.h"
#include "imagemagick_decoder.h"
#include "job.h"

using std::list;
using std::cout;
using std::vector;
using std::min;
using std::max;
using std::string;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;
using boost::lexical_cast;

Playlist::Playlist ()
	: _audio_from (AUDIO_FFMPEG)
{

}

void
Playlist::setup (ContentList content)
{
	_audio_from = AUDIO_FFMPEG;

	_video.clear ();
	_sndfile.clear ();

	for (list<boost::signals2::connection>::iterator i = _content_connections.begin(); i != _content_connections.end(); ++i) {
		i->disconnect ();
	}
	
	_content_connections.clear ();

	for (ContentList::const_iterator i = content.begin(); i != content.end(); ++i) {
		shared_ptr<VideoContent> vc = dynamic_pointer_cast<VideoContent> (*i);
		if (vc) {
			_video.push_back (vc);
		}
		
		shared_ptr<SndfileContent> sc = dynamic_pointer_cast<SndfileContent> (*i);
		if (sc) {
			_sndfile.push_back (sc);
			_audio_from = AUDIO_SNDFILE;
		}

		_content_connections.push_back ((*i)->Changed.connect (bind (&Playlist::content_changed, this, _1, _2)));
	}

	Changed ();
}

ContentAudioFrame
Playlist::audio_length () const
{
	ContentAudioFrame len = 0;
	
	switch (_audio_from) {
	case AUDIO_FFMPEG:
		for (list<shared_ptr<const VideoContent> >::const_iterator i = _video.begin(); i != _video.end(); ++i) {
			shared_ptr<const FFmpegContent> fc = dynamic_pointer_cast<const FFmpegContent> (*i);
			if (fc) {
				len += fc->audio_length ();
			}
		}
		break;
	case AUDIO_SNDFILE:
		for (list<shared_ptr<const SndfileContent> >::const_iterator i = _sndfile.begin(); i != _sndfile.end(); ++i) {
			len += (*i)->audio_length ();
		}
		break;
	}

	return len;
}

int
Playlist::audio_channels () const
{
	int channels = 0;
	
	switch (_audio_from) {
	case AUDIO_FFMPEG:
		for (list<shared_ptr<const VideoContent> >::const_iterator i = _video.begin(); i != _video.end(); ++i) {
			shared_ptr<const FFmpegContent> fc = dynamic_pointer_cast<const FFmpegContent> (*i);
			if (fc) {
				channels = max (channels, fc->audio_channels ());
			}
		}
		break;
	case AUDIO_SNDFILE:
		for (list<shared_ptr<const SndfileContent> >::const_iterator i = _sndfile.begin(); i != _sndfile.end(); ++i) {
			channels += (*i)->audio_channels ();
		}
		break;
	}

	return channels;
}

int
Playlist::audio_frame_rate () const
{
	/* XXX: assuming that all content has the same rate */
	
	switch (_audio_from) {
	case AUDIO_FFMPEG:
	{
		shared_ptr<const FFmpegContent> fc = first_ffmpeg ();
		if (fc) {
			return fc->audio_frame_rate ();
		}
		break;
	}
	case AUDIO_SNDFILE:
		return _sndfile.front()->audio_frame_rate ();
	}

	return 0;
}

float
Playlist::video_frame_rate () const
{
	if (_video.empty ()) {
		return 0;
	}
	
	/* XXX: assuming all the same */
	return _video.front()->video_frame_rate ();
}

libdcp::Size
Playlist::video_size () const
{
	if (_video.empty ()) {
		return libdcp::Size ();
	}

	/* XXX: assuming all the same */
	return _video.front()->video_size ();
}

ContentVideoFrame
Playlist::video_length () const
{
	ContentVideoFrame len = 0;
	for (list<shared_ptr<const VideoContent> >::const_iterator i = _video.begin(); i != _video.end(); ++i) {
		len += (*i)->video_length ();
	}
	
	return len;
}

bool
Playlist::has_audio () const
{
	/* XXX */
	return true;
}

void
Playlist::content_changed (weak_ptr<Content> c, int p)
{
	ContentChanged (c, p);
}

shared_ptr<const FFmpegContent>
Playlist::first_ffmpeg () const
{
	for (list<shared_ptr<const VideoContent> >::const_iterator i = _video.begin(); i != _video.end(); ++i) {
		shared_ptr<const FFmpegContent> fc = dynamic_pointer_cast<const FFmpegContent> (*i);
		if (fc) {
			return fc;
		}
	}

	return shared_ptr<const FFmpegContent> ();
}
	

AudioMapping
Playlist::default_audio_mapping () const
{
	AudioMapping m;

	switch (_audio_from) {
	case AUDIO_FFMPEG:
	{
		shared_ptr<const FFmpegContent> fc = first_ffmpeg ();
		if (!fc) {
			break;
		}
		
		/* XXX: assumes all the same */
		if (fc->audio_channels() == 1) {
			/* Map mono sources to centre */
			m.add (AudioMapping::Channel (fc, 0), libdcp::CENTRE);
		} else {
			int const N = min (fc->audio_channels (), MAX_AUDIO_CHANNELS);
			/* Otherwise just start with a 1:1 mapping */
			for (int i = 0; i < N; ++i) {
				m.add (AudioMapping::Channel (fc, i), (libdcp::Channel) i);
			}
		}
		break;
	}

	case AUDIO_SNDFILE:
	{
		int n = 0;
		for (list<shared_ptr<const SndfileContent> >::const_iterator i = _sndfile.begin(); i != _sndfile.end(); ++i) {
			for (int j = 0; j < (*i)->audio_channels(); ++j) {
				m.add (AudioMapping::Channel (*i, j), (libdcp::Channel) n);
				++n;
				if (n >= MAX_AUDIO_CHANNELS) {
					break;
				}
			}
			if (n >= MAX_AUDIO_CHANNELS) {
				break;
			}
		}
		break;
	}
	}

	return m;
}

string
Playlist::audio_digest () const
{
	string t;
	
	switch (_audio_from) {
	case AUDIO_FFMPEG:
		for (list<shared_ptr<const VideoContent> >::const_iterator i = _video.begin(); i != _video.end(); ++i) {
			shared_ptr<const FFmpegContent> fc = dynamic_pointer_cast<const FFmpegContent> (*i);
			if (fc) {
				t += (*i)->digest ();
				t += lexical_cast<string> (fc->audio_stream()->id);
			}
		}
		break;
	case AUDIO_SNDFILE:
		for (list<shared_ptr<const SndfileContent> >::const_iterator i = _sndfile.begin(); i != _sndfile.end(); ++i) {
			t += (*i)->digest ();
		}
		break;
	}

	return md5_digest (t.c_str(), t.length());
}

string
Playlist::video_digest () const
{
	string t;
	
	for (list<shared_ptr<const VideoContent> >::const_iterator i = _video.begin(); i != _video.end(); ++i) {
		t += (*i)->digest ();
		shared_ptr<const FFmpegContent> fc = dynamic_pointer_cast<const FFmpegContent> (*i);
		if (fc && fc->subtitle_stream()) {
			t += fc->subtitle_stream()->id;
		}
	}

	return md5_digest (t.c_str(), t.length());
}
