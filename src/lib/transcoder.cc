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

using std::string;
using boost::shared_ptr;

/** Construct a transcoder using a Decoder that we create and a supplied Encoder.
 *  @param f Film that we are transcoding.
 *  @param o Options.
 *  @param j Job that we are running under, or 0.
 *  @param e Encoder to use.
 */
Transcoder::Transcoder (shared_ptr<Film> f, shared_ptr<const Options> o, Job* j, shared_ptr<Encoder> e)
	: _job (j)
	, _encoder (e)
	, _decoder (decoder_factory (f, o, j))
{
	assert (_encoder);
	
	_decoder->Video.connect (bind (&Encoder::process_video, e, _1, _2, _3));
	_decoder->Audio.connect (bind (&Encoder::process_audio, e, _1));
}

/** Run the decoder, passing its output to the encoder, until the decoder
 *  has no more data to present.
 */
void
Transcoder::go ()
{
	_encoder->process_begin (_decoder->audio_channel_layout());
	try {
		_decoder->go ();
	} catch (...) {
		/* process_end() is important as the decoder may have worker
		   threads that need to be cleaned up.
		*/
		_encoder->process_end ();
		throw;
	}

	_encoder->process_end ();
}
