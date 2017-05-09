/*
    Copyright (C) 2012-2017 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/

/** @file  src/transcoder.cc
 *  @brief A class which takes a Film and some Options, then uses those to transcode the film.
 *
 *  A decoder is selected according to the content type, and the encoder can be specified
 *  as a parameter to the constructor.
 */

#include "transcoder.h"
#include "player.h"

#include "i18n.h"

using boost::weak_ptr;
using boost::shared_ptr;

/** Construct a transcoder.
 *  @param film Film that we are transcoding.
 *  @param job Job that this transcoder is being used in.
 */
Transcoder::Transcoder (shared_ptr<const Film> film, weak_ptr<Job> job)
	: _film (film)
	, _job (job)
	, _player (new Player (film, film->playlist ()))
{
	_player->Video.connect (bind (&Transcoder::video, this, _1, _2));
	_player->Audio.connect (bind (&Transcoder::audio, this, _1, _2));
	_player->Subtitle.connect (bind (&Transcoder::subtitle, this, _1, _2));
}
