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

using std::list;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

Playlist::Playlist (shared_ptr<const Film> f, list<shared_ptr<Content> > c)
	: _video_from (VIDEO_NONE)
	, _audio_from (AUDIO_NONE)
	, _ffmpeg_decoder_done (false)
{
	for (list<shared_ptr<Content> >::const_iterator i = c.begin(); i != c.end(); ++i) {
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

	if (_video_from == VIDEO_FFMPEG || _audio_from == AUDIO_FFMPEG) {
		DecodeOptions o;
		/* XXX: decodeoptions */
		_ffmpeg_decoder.reset (new FFmpegDecoder (f, _ffmpeg, o));
	}
	
	if (_video_from == VIDEO_FFMPEG) {
		_ffmpeg_decoder->connect_video (shared_from_this ());
	}

	if (_audio_from == AUDIO_FFMPEG) {
		_ffmpeg_decoder->connect_audio (shared_from_this ());
	}

	if (_video_from == VIDEO_IMAGEMAGICK) {
		for (list<shared_ptr<ImageMagickContent> >::iterator i = _imagemagick.begin(); i != _imagemagick.end(); ++i) {
			DecodeOptions o;
			/* XXX: decodeoptions */
			shared_ptr<ImageMagickDecoder> d (new ImageMagickDecoder (f, *i, o));
			_imagemagick_decoders.push_back (d);
			d->connect_video (shared_from_this ());
		}

		_imagemagick_decoder = _imagemagick_decoders.begin ();
	}

	if (_audio_from == AUDIO_SNDFILE) {
		for (list<shared_ptr<SndfileContent> >::iterator i = _sndfile.begin(); i != _sndfile.end(); ++i) {
			DecodeOptions o;
			/* XXX: decodeoptions */
			shared_ptr<SndfileDecoder> d (new SndfileDecoder (f, *i, o));
			_sndfile_decoders.push_back (d);
			d->connect_audio (shared_from_this ());
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
		for (list<shared_ptr<SndfileContent> >::const_iterator i = _sndfile.begin(); i != _sndfile.end(); ++i) {
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
		for (list<shared_ptr<SndfileContent> >::const_iterator i = _sndfile.begin(); i != _sndfile.end(); ++i) {
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

bool
Playlist::has_audio () const
{
	return _audio_from != AUDIO_NONE;
}
		
void
Playlist::disable_video ()
{
	_video_from = VIDEO_NONE;
}

bool
Playlist::pass ()
{
	bool done = true;
	
	if (_video_from == VIDEO_FFMPEG || _audio_from == AUDIO_FFMPEG) {
		if (!_ffmpeg_decoder_done) {
			if (_ffmpeg_decoder->pass ()) {
				_ffmpeg_decoder_done = true;
			} else {
				done = false;
			}
		}
	}

	if (_video_from == VIDEO_IMAGEMAGICK) {
		if (_imagemagick_decoder != _imagemagick_decoders.end ()) {
			if ((*_imagemagick_decoder)->pass ()) {
				_imagemagick_decoder++;
			}

			if (_imagemagick_decoder != _imagemagick_decoders.end ()) {
				done = false;
			}
		}
	}

	/* XXX: sndfile */

	return done;
}

void
Playlist::set_progress (shared_ptr<Job> job)
{
	/* XXX */
}

void
Playlist::process_video (shared_ptr<Image> i, bool same, shared_ptr<Subtitle> s)
{
	Video (i, same, s);
}

void
Playlist::process_audio (shared_ptr<AudioBuffers> b)
{
	Audio (b);
}

