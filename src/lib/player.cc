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
#include "ffmpeg_content.h"
#include "imagemagick_decoder.h"
#include "imagemagick_content.h"
#include "sndfile_decoder.h"
#include "sndfile_content.h"
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
	
	if (_video_decoder != _video_decoders.end ()) {
		if ((*_video_decoder)->pass ()) {
			_video_decoder++;
		}
		
		if (_video_decoder != _video_decoders.end ()) {
			done = false;
		}
	}

	if (_playlist->audio_from() == Playlist::AUDIO_SNDFILE) {
		for (list<shared_ptr<SndfileDecoder> >::iterator i = _sndfile_decoders.begin(); i != _sndfile_decoders.end(); ++i) {
			if (!(*i)->pass ()) {
				done = false;
			}
		}

		Audio (_audio_buffers);
		_audio_buffers.reset ();
	}

	return done;
}

void
Player::set_progress (shared_ptr<Job> job)
{
	/* Assume progress can be divined from how far through the video we are */

	if (_video_decoder == _video_decoders.end() || !_playlist->video_length()) {
		return;
	}
	
	ContentVideoFrame p = 0;
	list<shared_ptr<VideoDecoder> >::iterator i = _video_decoders.begin ();
	while (i != _video_decoders.end() && i != _video_decoder) {
		p += (*i)->video_length ();
	}

	job->set_progress (float ((*_video_decoder)->video_frame ()) / _playlist->video_length ());
}

void
Player::process_video (shared_ptr<Image> i, bool same, shared_ptr<Subtitle> s)
{
	Video (i, same, s);
}

void
Player::process_audio (weak_ptr<const AudioContent> c, shared_ptr<AudioBuffers> b)
{
	AudioMapping mapping = _film->audio_mapping ();
	if (!_audio_buffers) {
		_audio_buffers.reset (new AudioBuffers (mapping.dcp_channels(), b->frames ()));
		_audio_buffers->make_silent ();
	}

	for (int i = 0; i < b->channels(); ++i) {
		list<libdcp::Channel> dcp = mapping.content_to_dcp (AudioMapping::Channel (c, i));
		for (list<libdcp::Channel>::iterator j = dcp.begin(); j != dcp.end(); ++j) {
			_audio_buffers->accumulate (b, i, static_cast<int> (*j));
		}
	}

	if (_playlist->audio_from() == Playlist::AUDIO_FFMPEG) {
		/* We can just emit this audio now as it will all be here */
		Audio (_audio_buffers);
		_audio_buffers.reset ();
	}
}

/** @return true on error */
bool
Player::seek (double t)
{
	if (!_have_valid_decoders) {
		setup_decoders ();
		_have_valid_decoders = true;
	}

	/* Find the decoder that contains this position */
	_video_decoder = _video_decoders.begin ();
	while (_video_decoder != _video_decoders.end ()) {
		double const this_length = double ((*_video_decoder)->video_length()) / _film->video_frame_rate ();
		if (t < this_length) {
			break;
		}
		t -= this_length;
		++_video_decoder;
	}
	
	if (_video_decoder != _video_decoders.end()) {
		(*_video_decoder)->seek (t);
	} else {
		return true;
	}

	/* XXX: don't seek audio because we don't need to... */

	return false;
}

void
Player::setup_decoders ()
{
	_video_decoders.clear ();
	_video_decoder = _video_decoders.end ();
	_sndfile_decoders.clear ();
	
	if (_video) {
		list<shared_ptr<const VideoContent> > vc = _playlist->video ();
		for (list<shared_ptr<const VideoContent> >::iterator i = vc.begin(); i != vc.end(); ++i) {

			shared_ptr<VideoDecoder> d;
			
			/* XXX: into content? */
			
			shared_ptr<const FFmpegContent> fc = dynamic_pointer_cast<const FFmpegContent> (*i);
			if (fc) {
				shared_ptr<FFmpegDecoder> fd (
					new FFmpegDecoder (
						_film, fc, _video,
						_audio && _playlist->audio_from() == Playlist::AUDIO_FFMPEG,
						_subtitles,
						_video_sync
						)
					);

				if (_playlist->audio_from() == Playlist::AUDIO_FFMPEG) {
					fd->Audio.connect (bind (&Player::process_audio, this, fc, _1));
				}

				d = fd;
			}

			shared_ptr<const ImageMagickContent> ic = dynamic_pointer_cast<const ImageMagickContent> (*i);
			if (ic) {
				d.reset (new ImageMagickDecoder (_film, ic));
			}

			d->connect_video (shared_from_this ());
			_video_decoders.push_back (d);
		}

		_video_decoder = _video_decoders.begin ();
	}

	if (_audio && _playlist->audio_from() == Playlist::AUDIO_SNDFILE) {
		list<shared_ptr<const SndfileContent> > sc = _playlist->sndfile ();
		for (list<shared_ptr<const SndfileContent> >::iterator i = sc.begin(); i != sc.end(); ++i) {
			shared_ptr<SndfileDecoder> d (new SndfileDecoder (_film, *i));
			_sndfile_decoders.push_back (d);
			d->Audio.connect (bind (&Player::process_audio, this, *i, _1));
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
	double t = 0;
	for (list<shared_ptr<VideoDecoder> >::const_iterator i = _video_decoders.begin(); i != _video_decoder; ++i) {
		t += (*i)->video_length() / (*i)->video_frame_rate ();
	}

	return t + (*_video_decoder)->last_content_time ();
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
			/* FFmpeg content length changes are serious; we need new decoders */
			_have_valid_decoders = false;
		}
	}
}

void
Player::playlist_changed ()
{
	_have_valid_decoders = false;
}
