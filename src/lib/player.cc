/* -*- c-basic-offset: 8; default-tab-width: 8; -*- */

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
#include "image.h"

using std::list;
using std::cout;
using std::min;
using std::vector;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;

struct Piece
{
	Piece (shared_ptr<Content> c, shared_ptr<Decoder> d)
		: content (c)
		, decoder (d)
	{}
	
	shared_ptr<Content> content;
	shared_ptr<Decoder> decoder;
};

Player::Player (shared_ptr<const Film> f, shared_ptr<const Playlist> p)
	: _film (f)
	, _playlist (p)
	, _video (true)
	, _audio (true)
	, _subtitles (true)
	, _have_valid_pieces (false)
	, _position (0)
	, _audio_buffers (MAX_AUDIO_CHANNELS, 0)
	, _last_video (0)
	, _last_was_black (false)
	, _next_audio (0)
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
	if (!_have_valid_pieces) {
		setup_pieces ();
		_have_valid_pieces = true;
	}

        /* Here we are just finding the active decoder with the earliest last emission time, then
           calling pass on it.
        */

        Time earliest_t = TIME_MAX;
        shared_ptr<Piece> earliest;

	for (list<shared_ptr<Piece> >::iterator i = _pieces.begin(); i != _pieces.end(); ++i) {
		if ((*i)->content->end(_film) < _position) {
			continue;
		}
		
		Time const t = (*i)->content->start() + (*i)->decoder->next();
		if (t < earliest_t) {
			earliest_t = t;
			earliest = *i;
		}
	}

	if (!earliest) {
		return true;
	}
	
	earliest->decoder->pass ();

	/* Move position to earliest active next emission */

	for (list<shared_ptr<Piece> >::iterator i = _pieces.begin(); i != _pieces.end(); ++i) {
		if ((*i)->content->end(_film) < _position) {
			continue;
		}

		Time const t = (*i)->content->start() + (*i)->decoder->next();

		if (t < _position) {
			_position = t;
		}
	}

        return false;
}

void
Player::process_video (shared_ptr<Piece> piece, shared_ptr<const Image> image, bool same, shared_ptr<Subtitle> sub, Time time)
{
	time += piece->content->start ();
	
        Video (image, same, sub, time);
}

void
Player::process_audio (shared_ptr<Piece> piece, shared_ptr<const AudioBuffers> audio, Time time)
{
        /* XXX: mapping */

        /* The time of this audio may indicate that some of our buffered audio is not going to
           be added to any more, so it can be emitted.
        */

	time += piece->content->start ();

        if (time > _next_audio) {
                /* We can emit some audio from our buffers */
                OutputAudioFrame const N = min (_film->time_to_audio_frames (time - _next_audio), static_cast<OutputAudioFrame> (_audio_buffers.frames()));
                shared_ptr<AudioBuffers> emit (new AudioBuffers (_audio_buffers.channels(), N));
                emit->copy_from (&_audio_buffers, N, 0, 0);
                Audio (emit, _next_audio);
                _next_audio += _film->audio_frames_to_time (N);

                /* And remove it from our buffers */
                if (_audio_buffers.frames() > N) {
                        _audio_buffers.move (N, 0, _audio_buffers.frames() - N);
                }
                _audio_buffers.set_frames (_audio_buffers.frames() - N);
        }

        /* Now accumulate the new audio into our buffers */
        _audio_buffers.ensure_size (time - _next_audio + audio->frames());
        _audio_buffers.accumulate (audio.get(), 0, _film->time_to_audio_frames (time - _next_audio));
}

/** @return true on error */
void
Player::seek (Time t)
{
	if (!_have_valid_pieces) {
		setup_pieces ();
		_have_valid_pieces = true;
	}

	if (_pieces.empty ()) {
		return;
	}

	/* XXX: don't seek audio because we don't need to... */
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

struct ContentSorter
{
	bool operator() (shared_ptr<Content> a, shared_ptr<Content> b)
	{
		return a->start() < b->start();
	}
};

void
Player::setup_decoders ()
{
	list<shared_ptr<Piece> > old_pieces = _pieces;

	_pieces.clear ();

	Playlist::ContentList content = _playlist->content ();
	content.sort (ContentSorter ());
	
	for (Playlist::ContentList::iterator i = content.begin(); i != content.end(); ++i) {

		shared_ptr<Decoder> decoder;
		
                /* XXX: into content? */

		shared_ptr<const FFmpegContent> fc = dynamic_pointer_cast<const FFmpegContent> (*i);
		if (fc) {
			shared_ptr<FFmpegDecoder> fd (new FFmpegDecoder (_film, fc, _video, _audio, _subtitles));
			
			fd->Video.connect (bind (&Player::process_video, this, dr, _1, _2, _3, _4));
			fd->Audio.connect (bind (&Player::process_audio, this, dr, _1, _2));

			decoder = fd;
		}
		
		shared_ptr<const ImageMagickContent> ic = dynamic_pointer_cast<const ImageMagickContent> (*i);
		if (ic) {
			shared_ptr<ImageMagickDecoder> id;
			
			/* See if we can re-use an old ImageMagickDecoder */
			for (list<shared_ptr<Piece> >::const_iterator i = old_pieces.begin(); i != old_pieces.end(); ++i) {
				shared_ptr<ContentPiece> cp = dynamic_pointer_cast<ContentPiece> (*i);
				if (cp) {
					shared_ptr<ImageMagickDecoder> imd = dynamic_pointer_cast<ImageMagickDecoder> (cp->decoder ());
					if (imd && imd->content() == ic) {
						id = imd;
					}
				}
			}

			if (!id) {
				id.reset (new ImageMagickDecoder (_film, ic));
				id->Video.connect (bind (&Player::process_video, this, dr, _1, _2, _3, _4));
			}

			decoder = id;
		}

		shared_ptr<const SndfileContent> sc = dynamic_pointer_cast<const SndfileContent> (*i);
		if (sc) {
			shared_ptr<AudioDecoder> sd (new SndfileDecoder (_film, sc));
			sd->Audio.connect (bind (&Player::process_audio, this, dr, _1, _2));

			decoder = sd;
		}

		_pieces.push_back (shared_ptr<new ContentPiece> (*i, decoder));
	}

	/* Fill in visual gaps with black and audio gaps with silence */

	Time video_pos = 0;
	Time audio_pos = 0;
	list<shared_ptr<Piece> > pieces_copy = _pieces;
	for (list<shared_ptr<Piece> >::iterator i = pieces_copy.begin(); i != pieces_copy.end(); ++i) {
		if (dynamic_pointer_cast<VideoContent> ((*i)->content)) {
			Time const diff = video_pos - (*i)->content->time();
			if (diff > 0) {
				_pieces.push_back (
					shared_ptr<Piece> (
						shared_ptr<Content> (new NullContent (video_pos, diff)),
						shared_ptr<Decoder> (new BlackDecoder (video_pos, diff))
						)
					);
			}
						
			video_pos = (*i)->content->time() + (*i)->content->length();
		} else {
			Time const diff = audio_pos - (*i)->content->time();
			if (diff > 0) {
				_pieces.push_back (
					shared_ptr<Piece> (
						shared_ptr<Content> (new NullContent (audio_pos, diff)),
						shared_ptr<Decoder> (new SilenceDecoder (audio_pos, diff))
						)
					);
			}
			audio_pos = (*i)->content->time() + (*i)->content->length();
		}
	}

	_position = 0;
}

void
Player::content_changed (weak_ptr<Content> w, int p)
{
	shared_ptr<Content> c = w.lock ();
	if (!c) {
		return;
	}

	if (p == VideoContentProperty::VIDEO_LENGTH) {
		_have_valid_pieces = false;
	}
}

void
Player::playlist_changed ()
{
	_have_valid_pieces = false;
}

void
Player::emit_black_frame ()
{
	shared_ptr<SimpleImage> image (new SimpleImage (AV_PIX_FMT_RGB24, libdcp::Size (128, 128), true));
	Video (image, _last_was_black, shared_ptr<Subtitle> (), _last_video);
	_last_video += _film->video_frames_to_time (1);
}

void
Player::emit_silence (Time t)
{
	OutputAudioFrame frames = _film->time_to_audio_frames (t);
	while (frames) {
		/* Do this in half-second chunks so we don't overwhelm anybody */
		OutputAudioFrame this_time = min (_film->dcp_audio_frame_rate() / 2, frames);
		shared_ptr<AudioBuffers> silence (new AudioBuffers (MAX_AUDIO_CHANNELS, this_time));
		silence->make_silent ();
		Audio (silence, _last_audio);
		_last_audio += _film->audio_frames_to_time (this_time);
		frames -= this_time;
	}
}
