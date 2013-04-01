/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

/** @file  src/transcoder.cc
 *  @brief A class which takes a Film and some Options, then uses those to transcode the film.
 *
 *  A decoder is selected according to the content type, and the encoder can be specified
 *  as a parameter to the constructor.
 */

#include <iostream>
#include <boost/signals2.hpp>
#include "transcoder.h"
#include "encoder.h"
#include "film.h"
#include "matcher.h"
#include "delay_line.h"
#include "gain.h"
#include "video_decoder.h"
#include "audio_decoder.h"
#include "playlist.h"

using std::string;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

/** Construct a transcoder using a Decoder that we create and a supplied Encoder.
 *  @param f Film that we are transcoding.
 *  @param j Job that we are running under, or 0.
 *  @param e Encoder to use.
 */
Transcoder::Transcoder (shared_ptr<Film> f, shared_ptr<Job> j)
	: _job (j)
	, _playlist (f->playlist ())
	, _encoder (new Encoder (f, _playlist))
{
	if (_playlist->has_audio ()) {
		_matcher.reset (new Matcher (f->log(), _playlist->audio_frame_rate(), _playlist->video_frame_rate()));
		_delay_line.reset (new DelayLine (f->log(), _playlist->audio_channels(), f->audio_delay() * _playlist->audio_frame_rate() / 1000));
		_gain.reset (new Gain (f->log(), f->audio_gain()));
	}

	if (_matcher) {
		_playlist->connect_video (_matcher);
		_matcher->connect_video (_encoder);
	} else {
		_playlist->connect_video (_encoder);
	}
	
	if (_matcher && _delay_line && _playlist->has_audio ()) {
		_playlist->connect_audio (_delay_line);
		_delay_line->connect_audio (_matcher);
		_matcher->connect_audio (_gain);
		_gain->connect_audio (_encoder);
	}
}

void
Transcoder::go ()
{
	_encoder->process_begin ();
	try {
		while (1) {
			if (_playlist->pass ()) {
				break;
			}
			_playlist->set_progress (_job);
		}
		
	} catch (...) {
		_encoder->process_end ();
		throw;
	}
	
	if (_delay_line) {
		_delay_line->process_end ();
	}
	if (_matcher) {
		_matcher->process_end ();
	}
	if (_gain) {
		_gain->process_end ();
	}
	_encoder->process_end ();
}
