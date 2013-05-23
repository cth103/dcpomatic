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

ABTranscoder::ABTranscoder (shared_ptr<Film> film_a, shared_ptr<Film> film_b, shared_ptr<Job> j)
	: _player_a (film_a->player ())
	, _player_b (film_b->player ())
	, _job (j)
	, _encoder (new Encoder (film_a, j))
	, _combiner (new Combiner)
{
	_player_a->Video.connect (bind (&Combiner::process_video, _combiner, _1, _2, _3, _4));
	_player_b->Video.connect (bind (&Combiner::process_video_b, _combiner, _1, _2, _3, _4));

	_combiner->connect_video (_encoder);
	_player_a->connect_audio (_encoder);
}

void
ABTranscoder::go ()
{
	_encoder->process_begin ();
	while (!_player_a->pass () || !_player_b->pass ()) {}
	_encoder->process_end ();
}
