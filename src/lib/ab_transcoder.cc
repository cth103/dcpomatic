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

#include <iostream>
#include <boost/shared_ptr.hpp>
#include "ab_transcoder.h"
#include "film.h"
#include "encoder.h"
#include "job.h"
#include "image.h"
#include "player.h"
#include "matcher.h"
#include "delay_line.h"
#include "gain.h"
#include "combiner.h"

/** @file src/ab_transcoder.cc
 *  @brief A transcoder which uses one Film for the left half of the screen, and a different one
 *  for the right half (to facilitate A/B comparisons of settings)
 */

using std::string;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

/** @param a Film to use for the left half of the screen.
 *  @param b Film to use for the right half of the screen.
 *  @param o Decoder options.
 *  @param j Job that we are associated with.
 *  @param e Encoder to use.
 */

ABTranscoder::ABTranscoder (shared_ptr<Film> a, shared_ptr<Film> b, shared_ptr<Job> j)
	: _film_a (a)
	, _film_b (b)
	, _player_a (_film_a->player ())
	, _player_b (_film_b->player ())
	, _job (j)
	, _encoder (new Encoder (_film_a))
	, _combiner (new Combiner (a->log()))
{
	if (_film_a->has_audio ()) {
		_matcher.reset (new Matcher (_film_a->log(), _film_a->audio_frame_rate(), _film_a->video_frame_rate()));
		_delay_line.reset (new DelayLine (_film_a->log(), _film_a->audio_channels(), _film_a->audio_delay() * _film_a->audio_frame_rate() / 1000));
		_gain.reset (new Gain (_film_a->log(), _film_a->audio_gain()));
	}

	_player_a->Video.connect (bind (&Combiner::process_video, _combiner, _1, _2, _3));
	_player_b->Video.connect (bind (&Combiner::process_video_b, _combiner, _1, _2, _3));

	if (_matcher) {
		_combiner->connect_video (_matcher);
		_matcher->connect_video (_encoder);
	} else {
		_combiner->connect_video (_encoder);
	}
	
	if (_matcher && _delay_line) {
		_player_a->connect_audio (_delay_line);
		_delay_line->connect_audio (_matcher);
		_matcher->connect_audio (_gain);
		_gain->connect_audio (_encoder);
	}
}

void
ABTranscoder::go ()
{
	_encoder->process_begin ();

	bool done[2] = { false, false };
	
	while (1) {
		done[0] = _player_a->pass ();
		done[1] = _player_b->pass ();

		if (_job) {
			_player_a->set_progress (_job);
		}

		if (done[0] && done[1]) {
			break;
		}
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
			    
