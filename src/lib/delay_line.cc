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

#include <stdint.h>
#include <cstring>
#include <algorithm>
#include <iostream>
#include "delay_line.h"
#include "util.h"

using std::min;
using boost::shared_ptr;

/*  @param seconds Delay in seconds, +ve to move audio later.
 */
DelayLine::DelayLine (Log* log, double seconds)
	: Processor (log)
	, _seconds (seconds)
{
	
}

void
DelayLine::process_audio (shared_ptr<AudioBuffers> data, double t)
{
	if (_seconds > 0) {
		t += _seconds;
	}

	Audio (data, t);
}

void
DelayLine::process_video (boost::shared_ptr<Image> image, bool same, boost::shared_ptr<Subtitle> sub, double t)
{
	if (_seconds < 0) {
		t += _seconds;
	}

	Video (image, same, sub, t);
}
