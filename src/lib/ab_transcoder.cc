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
#include "trimmer.h"

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

ABTranscoder::ABTranscoder (
	shared_ptr<Film> a, shared_ptr<Film> b, DecodeOptions o, Job* j, shared_ptr<Encoder> e)
	: _film_a (a)
	, _film_b (b)
	, _job (j)
	, _encoder (e)
	, _combiner (new Combiner (a->log()))
{
	_da = decoder_factory (_film_a, o);
	_db = decoder_factory (_film_b, o);

	shared_ptr<AudioStream> st = _film_a->audio_stream();
	if (st) {
		_matcher.reset (new Matcher (_film_a->log(), st->sample_rate(), _film_a->source_frame_rate()));
	}
	_delay_line.reset (new DelayLine (_film_a->log(), _film_a->audio_delay() / 1000.0f));
	_gain.reset (new Gain (_film_a->log(), _film_a->audio_gain()));

	int const sr = st ? st->sample_rate() : 0;
	int const trim_start = _film_a->trim_type() == Film::ENCODE ? _film_a->trim_start() : 0;
	int const trim_end = _film_a->trim_type() == Film::ENCODE ? _film_a->trim_end() : 0;
	_trimmer.reset (new Trimmer (
				_film_a->log(), trim_start, trim_end, _film_a->length().get(),
				sr, _film_a->source_frame_rate(), _film_a->dcp_frame_rate()
				));
	
	/* Set up the decoder to use the film's set streams */
	_da.video->set_subtitle_stream (_film_a->subtitle_stream ());
	_db.video->set_subtitle_stream (_film_a->subtitle_stream ());
	if (_film_a->audio_stream ()) {
		_da.audio->set_audio_stream (_film_a->audio_stream ());
	}

	_da.video->Video.connect (bind (&Combiner::process_video, _combiner, _1, _2, _3, _4));
	_db.video->Video.connect (bind (&Combiner::process_video_b, _combiner, _1, _2, _3, _4));

	_combiner->connect_video (_delay_line);
	if (_matcher) {
		_delay_line->connect_video (_matcher);
		_matcher->connect_video (_trimmer);
	} else {
		_delay_line->connect_video (_trimmer);
	}
	_trimmer->connect_video (_encoder);
	
	_da.audio->connect_audio (_delay_line);
	if (_matcher) {
		_delay_line->connect_audio (_matcher);
		_matcher->connect_audio (_gain);
	} else {
		_delay_line->connect_audio (_gain);
	}
	_gain->connect_audio (_trimmer);
	_trimmer->connect_audio (_encoder);
}

void
ABTranscoder::go ()
{
	_encoder->process_begin ();

	bool done[3] = { false, false, false };
	
	while (1) {
		done[0] = _da.video->pass ();
		done[1] = _db.video->pass ();
		
		if (!done[2] && _da.audio && dynamic_pointer_cast<Decoder> (_da.audio) != dynamic_pointer_cast<Decoder> (_da.video)) {
			done[2] = _da.audio->pass ();
		} else {
			done[2] = true;
		}

		if (_job) {
			_da.video->set_progress (_job);
		}

		if (done[0] && done[1] && done[2]) {
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
			    
