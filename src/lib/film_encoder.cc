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


/** @file  src/film_encoder.cc
 *  @brief A class which takes a Film and some Options, then uses those to encode the film
 *  into some output format.
 *
 *  A decoder is selected according to the content type, and the encoder can be specified
 *  as a parameter to the constructor.
 */


#include "film_encoder.h"
#include "player.h"

#include "i18n.h"


/** Construct a FilmEncoder.
 *  @param film Film that we are encoding.
 *  @param job Job that this encoder is being used in.
 */
FilmEncoder::FilmEncoder(std::shared_ptr<const Film> film, std::weak_ptr<Job> job)
	: _film (film)
	, _job (job)
	, _player(film, Image::Alignment::PADDED)
{

}
