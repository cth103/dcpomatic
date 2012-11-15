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

#include "gain.h"

using boost::shared_ptr;

/** @param gain gain in dB */
Gain::Gain (Log* log, float gain)
	: AudioProcessor (log)
	, _gain (gain)
{

}

void
Gain::process_audio (shared_ptr<AudioBuffers> b)
{
	if (_gain != 0) {
		float const linear_gain = pow (10, _gain / 20);
		for (int i = 0; i < b->channels(); ++i) {
			for (int j = 0; j < b->frames(); ++j) {
				b->data(i)[j] *= linear_gain;
			}
		}
	}

	Audio (b);
}
