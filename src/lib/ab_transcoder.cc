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
#include "video_decoder.h"
#include "audio_decoder.h"
#include "encoder.h"
#include "job.h"
#include "options.h"
#include "image.h"
#include "decoder_factory.h"
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

/** @param a Film to use for the left half of the screen.
 *  @param b Film to use for the right half of the screen.
 *  @param o Decoder options.
 *  @param j Job that we are associated with.
 *  @param e Encoder to use.
 */

ABTranscoder::ABTranscoder (
	shared_ptr<Film> a, shared_ptr<Film> b, DecodeOptions o, Job* j, shared_ptr<Encoder> e)
	: _film_a (a)
	, _film_b (b)
	, _job (j)
	, _encoder (e)
{
	_da = decoder_factory (_film_a, o);
	_db = decoder_factory (_film_b, o);

	if (_film_a->audio_stream()) {
		shared_ptr<AudioStream> st = _film_a->audio_stream();
		_matcher.reset (new Matcher (_film_a->log(), st->sample_rate(), _film_a->source_frame_rate()));
		_delay_line.reset (new DelayLine (_film_a->log(), st->channels(), _film_a->audio_delay() * st->sample_rate() / 1000));
		_gain.reset (new Gain (_film_a->log(), _film_a->audio_gain()));
	}

	/* Set up the decoder to use the film's set streams */
	_da.video->set_subtitle_stream (_film_a->subtitle_stream ());
	_db.video->set_subtitle_stream (_film_a->subtitle_stream ());
	_da.audio->set_audio_stream (_film_a->audio_stream ());

	_da.video->Video.connect (bind (&Combiner::process_video, _combiner, _1, _2, _3));
	_db.video->Video.connect (bind (&Combiner::process_video_b, _combiner, _1, _2, _3));

	if (_matcher) {
		_combiner->connect_video (_matcher);
		_matcher->connect_video (_encoder);
	} else {
		_combiner->connect_video (_encoder);
	}
	
	if (_matcher && _delay_line) {
		_da.audio->connect_audio (_delay_line);
		_delay_line->connect_audio (_matcher);
		_matcher->connect_audio (_gain);
		_gain->connect_audio (_encoder);
	}
}

void
ABTranscoder::go ()
{
	_encoder->process_begin ();
	
	while (1) {
		bool const va = _da.video->pass ();
		bool const vb = _db.video->pass ();
		bool const a = _da.audio->pass ();

		if (_job) {
			_da.video->set_progress (_job);
		}

		if (va && vb && a) {
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
			    
