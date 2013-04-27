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
using std::vector;
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
	
	if (_video && _video_decoder < _video_decoders.size ()) {

		/* Run video decoder; this may also produce audio */
		
		if (_video_decoders[_video_decoder]->pass ()) {
			_video_decoder++;
		}
		
		if (_video_decoder < _video_decoders.size ()) {
			done = false;
		}
		
	}

	if (!_video && _audio && _playlist->audio_from() == Playlist::AUDIO_FFMPEG && _sequential_audio_decoder < _audio_decoders.size ()) {

		/* We're not producing video, so we may need to run FFmpeg content to get the audio */
		
		if (_audio_decoders[_sequential_audio_decoder]->pass ()) {
			_sequential_audio_decoder++;
		}
		
		if (_sequential_audio_decoder < _audio_decoders.size ()) {
			done = false;
		}
		
	}

	if (_audio && _playlist->audio_from() == Playlist::AUDIO_SNDFILE) {
		
		/* We're getting audio from SndfileContent */
		
		for (vector<shared_ptr<AudioDecoder> >::iterator i = _audio_decoders.begin(); i != _audio_decoders.end(); ++i) {
			if (!(*i)->pass ()) {
				done = false;
			}
		}

		Audio (_audio_buffers, _audio_time.get());
		_audio_buffers.reset ();
		_audio_time = boost::none;
	}

	return done;
}

void
Player::set_progress (shared_ptr<Job> job)
{
	/* Assume progress can be divined from how far through the video we are */

	if (_video_decoder >= _video_decoders.size() || !_playlist->video_length()) {
		return;
	}

	job->set_progress ((_video_start[_video_decoder] + _video_decoders[_video_decoder]->video_frame()) / _playlist->video_length ());
}

void
Player::process_video (shared_ptr<const Image> i, bool same, shared_ptr<Subtitle> s, double t)
{
	Video (i, same, s, _video_start[_video_decoder] + t);
}

void
Player::process_audio (weak_ptr<const AudioContent> c, shared_ptr<const AudioBuffers> b, double t)
{
	AudioMapping mapping = _film->audio_mapping ();
	if (!_audio_buffers) {
		_audio_buffers.reset (new AudioBuffers (mapping.dcp_channels(), b->frames ()));
		_audio_buffers->make_silent ();
		_audio_time = t;
		if (_playlist->audio_from() == Playlist::AUDIO_FFMPEG) {
			_audio_time = _audio_time.get() + _audio_start[_sequential_audio_decoder];
		}
	}

	for (int i = 0; i < b->channels(); ++i) {
		list<libdcp::Channel> dcp = mapping.content_to_dcp (AudioMapping::Channel (c, i));
		for (list<libdcp::Channel>::iterator j = dcp.begin(); j != dcp.end(); ++j) {
			_audio_buffers->accumulate (b, i, static_cast<int> (*j));
		}
	}

	if (_playlist->audio_from() == Playlist::AUDIO_FFMPEG) {
		/* We can just emit this audio now as it will all be here */
		Audio (_audio_buffers, t);
		_audio_buffers.reset ();
		_audio_time = boost::none;
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

	if (_video_decoders.empty ()) {
		return true;
	}

	/* Find the decoder that contains this position */
	_video_decoder = 0;
	while (1) {
		++_video_decoder;
		if (_video_decoder >= _video_decoders.size () || t < _video_start[_video_decoder]) {
			--_video_decoder;
			t -= _video_start[_video_decoder];
			break;
		}
	}

	if (_video_decoder < _video_decoders.size()) {
		_video_decoders[_video_decoder]->seek (t);
	} else {
		return true;
	}

	/* XXX: don't seek audio because we don't need to... */

	return false;
}


void
Player::seek_back ()
{
	/* XXX */
}

void
Player::seek_forward ()
{
	/* XXX */
}


void
Player::setup_decoders ()
{
	_video_decoders.clear ();
	_video_decoder = 0;
	_audio_decoders.clear ();
	_sequential_audio_decoder = 0;

	_video_start.clear();
	_audio_start.clear();

	double video_so_far = 0;
	double audio_so_far = 0;
	
	list<shared_ptr<const VideoContent> > vc = _playlist->video ();
	for (list<shared_ptr<const VideoContent> >::iterator i = vc.begin(); i != vc.end(); ++i) {
		
		shared_ptr<const VideoContent> video_content;
		shared_ptr<const AudioContent> audio_content;
		shared_ptr<VideoDecoder> video_decoder;
		shared_ptr<AudioDecoder> audio_decoder;
		
		/* XXX: into content? */
		
		shared_ptr<const FFmpegContent> fc = dynamic_pointer_cast<const FFmpegContent> (*i);
		if (fc) {
			shared_ptr<FFmpegDecoder> fd (
				new FFmpegDecoder (
					_film, fc, _video,
					_audio && _playlist->audio_from() == Playlist::AUDIO_FFMPEG,
					_subtitles
					)
				);
			
			video_content = fc;
			audio_content = fc;
			video_decoder = fd;
			audio_decoder = fd;
		}
		
		shared_ptr<const ImageMagickContent> ic = dynamic_pointer_cast<const ImageMagickContent> (*i);
		if (ic) {
			video_content = ic;
			video_decoder.reset (new ImageMagickDecoder (_film, ic));
		}
		
		video_decoder->connect_video (shared_from_this ());
		_video_decoders.push_back (video_decoder);
		_video_start.push_back (video_so_far);
		video_so_far += video_content->video_length() / video_content->video_frame_rate();

		if (audio_decoder && _playlist->audio_from() == Playlist::AUDIO_FFMPEG) {
			audio_decoder->Audio.connect (bind (&Player::process_audio, this, audio_content, _1, _2));
			_audio_decoders.push_back (audio_decoder);
			_audio_start.push_back (audio_so_far);
			audio_so_far += double(audio_content->audio_length()) / audio_content->audio_frame_rate();
		}
	}
	
	_video_decoder = 0;
	_sequential_audio_decoder = 0;

	if (_playlist->audio_from() == Playlist::AUDIO_SNDFILE) {
		
		list<shared_ptr<const AudioContent> > ac = _playlist->audio ();
		for (list<shared_ptr<const AudioContent> >::iterator i = ac.begin(); i != ac.end(); ++i) {
			
			shared_ptr<const SndfileContent> sc = dynamic_pointer_cast<const SndfileContent> (*i);
			assert (sc);
			
			shared_ptr<AudioDecoder> d (new SndfileDecoder (_film, sc));
			d->Audio.connect (bind (&Player::process_audio, this, sc, _1, _2));
			_audio_decoders.push_back (d);
			_audio_start.push_back (audio_so_far);
		}
	}
}

double
Player::last_video_time () const
{
	return _video_start[_video_decoder] + _video_decoders[_video_decoder]->last_content_time ();
}

void
Player::content_changed (weak_ptr<Content> w, int p)
{
	shared_ptr<Content> c = w.lock ();
	if (!c) {
		return;
	}

	if (p == VideoContentProperty::VIDEO_LENGTH) {
		_have_valid_decoders = false;
	}
}

void
Player::playlist_changed ()
{
	_have_valid_decoders = false;
}
