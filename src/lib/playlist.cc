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
#include "playlist.h"
#include "sndfile_content.h"
#include "sndfile_decoder.h"
#include "ffmpeg_content.h"
#include "ffmpeg_decoder.h"
#include "imagemagick_content.h"
#include "imagemagick_decoder.h"
#include "job.h"

using std::list;
using std::cout;
using std::vector;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

Playlist::Playlist ()
	: _video_from (VIDEO_NONE)
	, _audio_from (AUDIO_NONE)
{

}

void
Playlist::setup (ContentList content)
{
	_video_from = VIDEO_NONE;
	_audio_from = AUDIO_NONE;

	_ffmpeg.reset ();
	_imagemagick.clear ();
	_sndfile.clear ();

	for (ContentList::const_iterator i = content.begin(); i != content.end(); ++i) {
		shared_ptr<FFmpegContent> fc = dynamic_pointer_cast<FFmpegContent> (*i);
		if (fc) {
			assert (!_ffmpeg);
			_ffmpeg = fc;
			_video_from = VIDEO_FFMPEG;
			if (_audio_from == AUDIO_NONE) {
				_audio_from = AUDIO_FFMPEG;
			}
		}
		
		shared_ptr<ImageMagickContent> ic = dynamic_pointer_cast<ImageMagickContent> (*i);
		if (ic) {
			_imagemagick.push_back (ic);
			if (_video_from == VIDEO_NONE) {
				_video_from = VIDEO_IMAGEMAGICK;
			}
		}

		shared_ptr<SndfileContent> sc = dynamic_pointer_cast<SndfileContent> (*i);
		if (sc) {
			_sndfile.push_back (sc);
			_audio_from = AUDIO_SNDFILE;
		}
	}
}

ContentAudioFrame
Playlist::audio_length () const
{
	switch (_audio_from) {
	case AUDIO_NONE:
		return 0;
	case AUDIO_FFMPEG:
		return _ffmpeg->audio_length ();
	case AUDIO_SNDFILE:
	{
		ContentAudioFrame l = 0;
		for (list<shared_ptr<const SndfileContent> >::const_iterator i = _sndfile.begin(); i != _sndfile.end(); ++i) {
			l += (*i)->audio_length ();
		}
		return l;
	}
	}

	return 0;
}

int
Playlist::audio_channels () const
{
	switch (_audio_from) {
	case AUDIO_NONE:
		return 0;
	case AUDIO_FFMPEG:
		return _ffmpeg->audio_channels ();
	case AUDIO_SNDFILE:
	{
		int c = 0;
		for (list<shared_ptr<const SndfileContent> >::const_iterator i = _sndfile.begin(); i != _sndfile.end(); ++i) {
			c += (*i)->audio_channels ();
		}
		return c;
	}
	}

	return 0;
}

int
Playlist::audio_frame_rate () const
{
	switch (_audio_from) {
	case AUDIO_NONE:
		return 0;
	case AUDIO_FFMPEG:
		return _ffmpeg->audio_frame_rate ();
	case AUDIO_SNDFILE:
		return _sndfile.front()->audio_frame_rate ();
	}

	return 0;
}

int64_t
Playlist::audio_channel_layout () const
{
	switch (_audio_from) {
	case AUDIO_NONE:
		return 0;
	case AUDIO_FFMPEG:
		return _ffmpeg->audio_channel_layout ();
	case AUDIO_SNDFILE:
		/* XXX */
		return 0;
	}

	return 0;
}

float
Playlist::video_frame_rate () const
{
	switch (_video_from) {
	case VIDEO_NONE:
		return 0;
	case VIDEO_FFMPEG:
		return _ffmpeg->video_frame_rate ();
	case VIDEO_IMAGEMAGICK:
		return 24;
	}

	return 0;
}

libdcp::Size
Playlist::video_size () const
{
	switch (_video_from) {
	case VIDEO_NONE:
		return libdcp::Size ();
	case VIDEO_FFMPEG:
		return _ffmpeg->video_size ();
	case VIDEO_IMAGEMAGICK:
		/* XXX */
		return _imagemagick.front()->video_size ();
	}

	return libdcp::Size ();
}

ContentVideoFrame
Playlist::video_length () const
{
	switch (_video_from) {
	case VIDEO_NONE:
		return 0;
	case VIDEO_FFMPEG:
		return _ffmpeg->video_length ();
	case VIDEO_IMAGEMAGICK:
	{
		ContentVideoFrame l = 0;
		for (list<shared_ptr<const ImageMagickContent> >::const_iterator i = _imagemagick.begin(); i != _imagemagick.end(); ++i) {
			l += (*i)->video_length ();
		}
		return l;
	}
	}

	return 0;
}

bool
Playlist::has_audio () const
{
	return _audio_from != AUDIO_NONE;
}

