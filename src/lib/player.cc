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

Player::Player (shared_ptr<const Film> f, shared_ptr<const Playlist> p)
	: _film (f)
	, _playlist (p)
	, _video (true)
	, _audio (true)
	, _subtitles (true)
	, _have_valid_decoders (false)
	, _position (0)
	, _audio_buffers (MAX_AUDIO_CHANNELS, 0)
	, _last_video (0)
	, _last_was_black (false)
	, _last_audio (0)
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

        /* Here we are just finding the active decoder with the earliest last emission time, then
           calling pass on it.  If there is no decoder, we skip our position on until there is.
           Hence this method will cause video and audio to be emitted, and it is up to the
           process_{video,audio} methods to tidy it up.
        */

        Time earliest_pos = TIME_MAX;
        shared_ptr<RegionDecoder> earliest;
        Time next_wait = TIME_MAX;

        for (list<shared_ptr<RegionDecoder> >::iterator i = _decoders.begin(); i != _decoders.end(); ++i) {
                Time const ts = (*i)->region->time;
                Time const te = (*i)->region->time + (*i)->region->content->length (_film);
                if (ts <= _position && te > _position) {
                        Time const pos = ts + (*i)->last;
                        if (pos < earliest_pos) {
                                earliest_pos = pos;
                                earliest = *i;
                        }
                }

                if (ts > _position) {
                        next_wait = min (next_wait, ts - _position);
                }
        }

        if (earliest) {
                earliest->decoder->pass ();
                _position = earliest->last;
        } else if (next_wait < TIME_MAX) {
                _position += next_wait;
        } else {
                return true;
        }

        return false;
}

void
Player::process_video (shared_ptr<RegionDecoder> rd, shared_ptr<const Image> image, bool same, shared_ptr<Subtitle> sub, Time time)
{
        shared_ptr<VideoDecoder> vd = dynamic_pointer_cast<VideoDecoder> (rd->decoder);
        
        Time const global_time = rd->region->time + time;
        while ((global_time - _last_video) > 1) {
                /* Fill in with black */
                emit_black_frame ();
        }

        Video (image, same, sub, global_time);
        rd->last = time;
	_last_video = global_time;
	_last_was_black = false;
}

void
Player::process_audio (shared_ptr<RegionDecoder> rd, shared_ptr<const AudioBuffers> audio, Time time)
{
        /* XXX: mapping */

        /* The time of this audio may indicate that some of our buffered audio is not going to
           be added to any more, so it can be emitted.
        */

        if (time > _last_audio) {
                /* We can emit some audio from our buffers */
                OutputAudioFrame const N = min (_film->time_to_audio_frames (time - _last_audio), static_cast<OutputAudioFrame> (_audio_buffers.frames()));
                shared_ptr<AudioBuffers> emit (new AudioBuffers (_audio_buffers.channels(), N));
                emit->copy_from (&_audio_buffers, N, 0, 0);
                Audio (emit, _last_audio);
                _last_audio += _film->audio_frames_to_time (N);

                /* And remove it from our buffers */
                if (_audio_buffers.frames() > N) {
                        _audio_buffers.move (N, 0, _audio_buffers.frames() - N);
                }
                _audio_buffers.set_frames (_audio_buffers.frames() - N);
        }

        /* Now accumulate the new audio into our buffers */

        if (_audio_buffers.frames() == 0) {
                /* We have no remaining data.  Emit silence up to the start of this new data */
                if ((time - _last_audio) > 0) {
                        emit_silence (time - _last_audio);
                }
        }

        _audio_buffers.ensure_size (time - _last_audio + audio->frames());
        _audio_buffers.accumulate (audio.get(), 0, _film->time_to_audio_frames (time - _last_audio));
        rd->last = time + _film->audio_frames_to_time (audio->frames ());
}

/** @return true on error */
bool
Player::seek (Time t)
{
	if (!_have_valid_decoders) {
		setup_decoders ();
		_have_valid_decoders = true;
	}

	if (_decoders.empty ()) {
		return true;
	}

	/* XXX */

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
	list<shared_ptr<RegionDecoder> > old_decoders = _decoders;

	_decoders.clear ();

	Playlist::RegionList regions = _playlist->regions ();
	for (Playlist::RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {

		shared_ptr<RegionDecoder> rd (new RegionDecoder);
		rd->region = *i;
		
                /* XXX: into content? */

		shared_ptr<const FFmpegContent> fc = dynamic_pointer_cast<const FFmpegContent> ((*i)->content);
		if (fc) {
			shared_ptr<FFmpegDecoder> fd (new FFmpegDecoder (_film, fc, _video, _audio, _subtitles));
			
			fd->Video.connect (bind (&Player::process_video, this, rd, _1, _2, _3, _4));
			fd->Audio.connect (bind (&Player::process_audio, this, rd, _1, _2));

			rd->decoder = fd;
		}
		
		shared_ptr<const ImageMagickContent> ic = dynamic_pointer_cast<const ImageMagickContent> ((*i)->content);
		if (ic) {
			shared_ptr<ImageMagickDecoder> id;
			
			/* See if we can re-use an old ImageMagickDecoder */
			for (list<shared_ptr<RegionDecoder> >::const_iterator i = old_decoders.begin(); i != old_decoders.end(); ++i) {
				shared_ptr<ImageMagickDecoder> imd = dynamic_pointer_cast<ImageMagickDecoder> ((*i)->decoder);
				if (imd && imd->content() == ic) {
					id = imd;
				}
			}

			if (!id) {
				id.reset (new ImageMagickDecoder (_film, ic));
				id->Video.connect (bind (&Player::process_video, this, rd, _1, _2, _3, _4));
			}

			rd->decoder = id;
		}

		shared_ptr<const SndfileContent> sc = dynamic_pointer_cast<const SndfileContent> ((*i)->content);
		if (sc) {
			shared_ptr<AudioDecoder> sd (new SndfileDecoder (_film, sc));
			sd->Audio.connect (bind (&Player::process_audio, this, rd, _1, _2));

			rd->decoder = sd;
		}

		_decoders.push_back (rd);
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
		_have_valid_decoders = false;
	}
}

void
Player::playlist_changed ()
{
	_have_valid_decoders = false;
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
