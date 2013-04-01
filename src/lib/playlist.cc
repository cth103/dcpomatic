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
using std::cout;
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

Player::Player (boost::shared_ptr<const Film> f, boost::shared_ptr<const Playlist> p)
	: _film (f)
	, _playlist (p)
	, _video (true)
	, _audio (true)
	, _subtitles (true)
	, _have_setup_decoders (false)
	, _ffmpeg_decoder_done (false)
	, _video_sync (true)
{

}
		
void
Player::disable_video ()
{
	_video = false;
}

void
Player::disable_audio ()
{
	_audio = false;
}

void
Player::disable_subtitles ()
{
	_subtitles = false;
}

bool
Player::pass ()
{
	if (!_have_setup_decoders) {
		setup_decoders ();
		_have_setup_decoders = true;
	}
	
	bool done = true;
	
	if (_playlist->video_from() == Playlist::VIDEO_FFMPEG || _playlist->audio_from() == Playlist::AUDIO_FFMPEG) {
		if (!_ffmpeg_decoder_done) {
			if (_ffmpeg_decoder->pass ()) {
				_ffmpeg_decoder_done = true;
			} else {
				done = false;
			}
		}
	}

	if (_playlist->video_from() == Playlist::VIDEO_IMAGEMAGICK) {
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
Player::set_progress (shared_ptr<Job> job)
{
	/* XXX */
}

void
Player::process_video (shared_ptr<Image> i, bool same, shared_ptr<Subtitle> s)
{
	Video (i, same, s);
}

void
Player::process_audio (shared_ptr<AudioBuffers> b)
{
	Audio (b);
}

/** @return true on error */
bool
Player::seek (double t)
{
	bool r = false;

	switch (_playlist->video_from()) {
	case Playlist::VIDEO_NONE:
		break;
	case Playlist::VIDEO_FFMPEG:
		if (_ffmpeg_decoder->seek (t)) {
			r = true;
		}
		break;
	case Playlist::VIDEO_IMAGEMAGICK:
		/* Find the decoder that contains this position */
		_imagemagick_decoder = _imagemagick_decoders.begin ();
		while (_imagemagick_decoder != _imagemagick_decoders.end ()) {
			double const this_length = (*_imagemagick_decoder)->video_length() / _film->video_frame_rate ();
			if (this_length < t) {
				break;
			}
			t -= this_length;
			++_imagemagick_decoder;
		}

		if (_imagemagick_decoder != _imagemagick_decoders.end()) {
			(*_imagemagick_decoder)->seek (t);
		} else {
			r = true;
		}
		break;
	}

	/* XXX: don't seek audio because we don't need to... */

	return r;
}

bool
Player::seek_to_last ()
{
	bool r = false;
	
	switch (_playlist->video_from ()) {
	case Playlist::VIDEO_NONE:
		break;
	case Playlist::VIDEO_FFMPEG:
		if (_ffmpeg_decoder->seek_to_last ()) {
			r = true;
		}
		break;
	case Playlist::VIDEO_IMAGEMAGICK:
		if ((*_imagemagick_decoder)->seek_to_last ()) {
			r = true;
		}
		break;
	}

	/* XXX: don't seek audio because we don't need to... */

	return r;
}

void
Player::setup_decoders ()
{
	if ((_video && _playlist->video_from() == Playlist::VIDEO_FFMPEG) || (_audio && _playlist->audio_from() == Playlist::AUDIO_FFMPEG)) {
		_ffmpeg_decoder.reset (
			new FFmpegDecoder (
				_film,
				_playlist->ffmpeg(),
				_video && _playlist->video_from() == Playlist::VIDEO_FFMPEG,
				_audio && _playlist->audio_from() == Playlist::AUDIO_FFMPEG,
				_subtitles && _film->with_subtitles(),
				_video_sync
				)
			);
	}
	
	if (_video && _playlist->video_from() == Playlist::VIDEO_FFMPEG) {
		_ffmpeg_decoder->connect_video (shared_from_this ());
	}

	if (_audio && _playlist->audio_from() == Playlist::AUDIO_FFMPEG) {
		_ffmpeg_decoder->connect_audio (shared_from_this ());
	}

	if (_video && _playlist->video_from() == Playlist::VIDEO_IMAGEMAGICK) {
		list<shared_ptr<const ImageMagickContent> > ic = _playlist->imagemagick ();
		for (list<shared_ptr<const ImageMagickContent> >::iterator i = ic.begin(); i != ic.end(); ++i) {
			shared_ptr<ImageMagickDecoder> d (new ImageMagickDecoder (_film, *i));
			_imagemagick_decoders.push_back (d);
			d->connect_video (shared_from_this ());
		}

		_imagemagick_decoder = _imagemagick_decoders.begin ();
	}

	if (_audio && _playlist->audio_from() == Playlist::AUDIO_SNDFILE) {
		list<shared_ptr<const SndfileContent> > sc = _playlist->sndfile ();
		for (list<shared_ptr<const SndfileContent> >::iterator i = sc.begin(); i != sc.end(); ++i) {
			shared_ptr<SndfileDecoder> d (new SndfileDecoder (_film, *i));
			_sndfile_decoders.push_back (d);
			d->connect_audio (shared_from_this ());
		}
	}
}

void
Player::disable_video_sync ()
{
	_video_sync = false;
}

double
Player::last_video_time () const
{
	switch (_playlist->video_from ()) {
	case Playlist::VIDEO_NONE:
		return 0;
	case Playlist::VIDEO_FFMPEG:
		return _ffmpeg_decoder->last_source_time ();
	case Playlist::VIDEO_IMAGEMAGICK:
		return (*_imagemagick_decoder)->last_source_time ();
	}

	return 0;
}
