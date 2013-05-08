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
#include "trimmer.h"

using std::string;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

/** Construct a transcoder using a Decoder that we create and a supplied Encoder.
 *  @param f Film that we are transcoding.
 *  @param o Decode options.
 *  @param j Job that we are running under, or 0.
 *  @param e Encoder to use.
 */
Transcoder::Transcoder (shared_ptr<Film> f, DecodeOptions o, Job* j, shared_ptr<Encoder> e)
	: _job (j)
	, _encoder (e)
	, _decoders (decoder_factory (f, o))
{
	assert (_encoder);

	shared_ptr<AudioStream> st = f->audio_stream();
	if (st && st->sample_rate ()) {
		_matcher.reset (new Matcher (f->log(), st->sample_rate(), f->source_frame_rate()));
	}
	_delay_line.reset (new DelayLine (f->log(), f->audio_delay() / 1000.0f));
	_gain.reset (new Gain (f->log(), f->audio_gain()));

	int const sr = st ? st->sample_rate() : 0;
	int const trim_start = f->trim_type() == Film::ENCODE ? f->trim_start() : 0;
	int const trim_end = f->trim_type() == Film::ENCODE ? f->trim_end() : 0;
	_trimmer.reset (new Trimmer (
				f->log(), trim_start, trim_end, f->length().get_value_or(0),
				sr, f->source_frame_rate(), f->dcp_frame_rate()
				));

	/* Set up the decoder to use the film's set streams */
	_decoders.video->set_subtitle_stream (f->subtitle_stream ());
	if (f->audio_stream ()) {
	    _decoders.audio->set_audio_stream (f->audio_stream ());
	}

	_decoders.video->connect_video (_delay_line);
	if (_matcher) {
		_delay_line->connect_video (_matcher);
		_matcher->connect_video (_trimmer);
	} else {
		_delay_line->connect_video (_trimmer);
	}
	_trimmer->connect_video (_encoder);
	
	_decoders.audio->connect_audio (_delay_line);
	if (_matcher) {
		_delay_line->connect_audio (_matcher);
		_matcher->connect_audio (_gain);
	} else {
		_delay_line->connect_audio (_gain);
	}
	_gain->connect_audio (_trimmer);
	_trimmer->connect_audio (_encoder);
}

/** Run the decoder, passing its output to the encoder, until the decoder
 *  has no more data to present.
 */
void
Transcoder::go ()
{
	_encoder->process_begin ();

	bool done[2] = { false, false };
	
	while (1) {
		if (!done[0]) {
			done[0] = _decoders.video->pass ();
			if (_job) {
				_decoders.video->set_progress (_job);
			}
		}
		
		if (!done[1] && _decoders.audio && dynamic_pointer_cast<Decoder> (_decoders.audio) != dynamic_pointer_cast<Decoder> (_decoders.video)) {
			done[1] = _decoders.audio->pass ();
		} else {
			done[1] = true;
		}
		
		if (done[0] && done[1]) {
			break;
		}
	}
	
	_delay_line->process_end ();
	if (_matcher) {
		_matcher->process_end ();
	}
	_gain->process_end ();
	_encoder->process_end ();
}
