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
#include "decoder_factory.h"
#include "film.h"
#include "matcher.h"
#include "delay_line.h"
#include "options.h"
#include "gain.h"
#include "video_decoder.h"
#include "audio_decoder.h"

using std::string;
using std::cout;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

/** Construct a transcoder using a Decoder that we create and a supplied Encoder.
 *  @param f Film that we are transcoding.
 *  @param o Options.
 *  @param j Job that we are running under, or 0.
 *  @param e Encoder to use.
 */
Transcoder::Transcoder (shared_ptr<Film> f, shared_ptr<const Options> o, Job* j, shared_ptr<Encoder> e)
	: _job (j)
	, _encoder (e)
	, _decoders (decoder_factory (f, o, j))
{
	assert (_encoder);

	if (f->audio_stream()) {
		shared_ptr<AudioStream> st = f->audio_stream();
		_matcher.reset (new Matcher (f->log(), st->sample_rate(), f->frames_per_second()));
		_delay_line.reset (new DelayLine (f->log(), st->channels(), f->audio_delay() * st->sample_rate() / 1000));
		_gain.reset (new Gain (f->log(), f->audio_gain()));
	}

	/* Set up the decoder to use the film's set streams */
	_decoders.first->set_subtitle_stream (f->subtitle_stream ());
	_decoders.second->set_audio_stream (f->audio_stream ());

	if (_matcher) {
		_decoders.first->connect_video (_matcher);
		_matcher->connect_video (_encoder);
	} else {
		_decoders.first->connect_video (_encoder);
	}
	
	if (_matcher && _delay_line) {
		_decoders.second->connect_audio (_delay_line);
		_delay_line->connect_audio (_matcher);
		_matcher->connect_audio (_gain);
		_gain->connect_audio (_encoder);
	}
}

/** Run the decoder, passing its output to the encoder, until the decoder
 *  has no more data to present.
 */
void
Transcoder::go ()
{
	_encoder->process_begin ();
	try {
		bool done[2] = { false, false };
		
		while (1) {
			if (!done[0]) {
				done[0] = _decoders.first->pass ();
				_decoders.first->set_progress ();
			}

			if (!done[1] && dynamic_pointer_cast<Decoder> (_decoders.second) != dynamic_pointer_cast<Decoder> (_decoders.first)) {
				done[1] = _decoders.second->pass ();
			} else {
				done[1] = true;
			}

			if (done[0] && done[1]) {
				break;
			}
		}
		
	} catch (...) {
		/* process_end() is important as the decoder may have worker
		   threads that need to be cleaned up.
		*/
		_encoder->process_end ();
		throw;
	}

	_encoder->process_end ();
}
