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

#include "player.h"
#include "film.h"
#include "ffmpeg_decoder.h"
#include "imagemagick_decoder.h"
#include "sndfile_decoder.h"
#include "playlist.h"
#include "job.h"

using std::list;
using std::cout;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;

Player::Player (shared_ptr<const Film> f, shared_ptr<const Playlist> p)
	: _film (f)
	, _playlist (p)
	, _video (true)
	, _audio (true)
	, _subtitles (true)
	, _have_valid_decoders (false)
	, _ffmpeg_decoder_done (false)
	, _video_sync (true)
{
	_playlist->Changed.connect (bind (&Player::playlist_changed, this));
	_playlist->ContentChanged.connect (bind (&Player::content_changed, this, _1, _2));
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
	if (!_have_valid_decoders) {
		setup_decoders ();
		_have_valid_decoders = true;
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
	/* Assume progress can be divined from how far through the video we are */
	switch (_playlist->video_from ()) {
	case Playlist::VIDEO_NONE:
		break;
	case Playlist::VIDEO_FFMPEG:
		if (_playlist->video_length ()) {
			job->set_progress (float(_ffmpeg_decoder->video_frame()) / _playlist->video_length ());
		}
		break;
	case Playlist::VIDEO_IMAGEMAGICK:
	{
		int n = 0;
		for (list<shared_ptr<ImageMagickDecoder> >::iterator i = _imagemagick_decoders.begin(); i != _imagemagick_decoders.end(); ++i) {
			if (_imagemagick_decoder == i) {
				job->set_progress (float (n) / _imagemagick_decoders.size ());
			}
			++n;
		}
		break;
	}
	}
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
	if (!_have_valid_decoders) {
		setup_decoders ();
		_have_valid_decoders = true;
	}
	
	bool r = false;

	switch (_playlist->video_from()) {
	case Playlist::VIDEO_NONE:
		break;
	case Playlist::VIDEO_FFMPEG:
		if (!_ffmpeg_decoder || _ffmpeg_decoder->seek (t)) {
			r = true;
		}
		/* We're seeking, so all `all done' bets are off */
		_ffmpeg_decoder_done = false;
		break;
	case Playlist::VIDEO_IMAGEMAGICK:
		/* Find the decoder that contains this position */
		_imagemagick_decoder = _imagemagick_decoders.begin ();
		while (_imagemagick_decoder != _imagemagick_decoders.end ()) {
			double const this_length = (*_imagemagick_decoder)->video_length() / _film->video_frame_rate ();
			if (t < this_length) {
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
	if (!_have_valid_decoders) {
		setup_decoders ();
		_have_valid_decoders = true;
	}

	bool r = false;
	
	switch (_playlist->video_from ()) {
	case Playlist::VIDEO_NONE:
		break;
	case Playlist::VIDEO_FFMPEG:
		if (!_ffmpeg_decoder || _ffmpeg_decoder->seek_to_last ()) {
			r = true;
		}

		/* We're seeking, so all `all done' bets are off */
		_ffmpeg_decoder_done = false;
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
	{
		double t = 0;
		for (list<shared_ptr<ImageMagickDecoder> >::const_iterator i = _imagemagick_decoders.begin(); i != _imagemagick_decoder; ++i) {
			t += (*i)->video_length() / (*i)->video_frame_rate ();
		}

		return t + (*_imagemagick_decoder)->last_source_time ();
	}
	}

	return 0;
}

void
Player::content_changed (weak_ptr<Content> w, int p)
{
	shared_ptr<Content> c = w.lock ();
	if (!c) {
		return;
	}

	if (p == VideoContentProperty::VIDEO_LENGTH) {
		if (dynamic_pointer_cast<FFmpegContent> (c)) {
			_have_valid_decoders = false;
		}
	}
}

void
Player::playlist_changed ()
{
	_have_valid_decoders = false;
}
