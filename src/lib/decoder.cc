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

/** @file  src/decoder.cc
 *  @brief Parent class for decoders of content.
 */

#include <iostream>
#include <stdint.h>
#include <boost/lexical_cast.hpp>
#include "film.h"
#include "format.h"
#include "job.h"
#include "options.h"
#include "exceptions.h"
#include "image.h"
#include "util.h"
#include "log.h"
#include "decoder.h"
#include "delay_line.h"
#include "subtitle.h"
#include "filter_graph.h"

using std::string;
using std::stringstream;
using std::min;
using std::pair;
using std::list;
using boost::shared_ptr;
using boost::optional;

/** @param f Film.
 *  @param o Options.
 *  @param j Job that we are running within, or 0
 */
Decoder::Decoder (boost::shared_ptr<Film> f, boost::shared_ptr<const Options> o, Job* j)
	: _film (f)
	, _opt (o)
	, _job (j)
{
	
}

bool
Decoder::seek (SourceFrame f)
{
	throw DecodeError ("decoder does not support seek");
}
