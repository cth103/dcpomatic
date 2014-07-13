/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include "audio_processor.h"
#include "mid_side_decoder.h"

using std::string;
using std::list;

list<AudioProcessor const *> AudioProcessor::_all;

void
AudioProcessor::setup_audio_processors ()
{
	_all.push_back (new MidSideDecoder ());
}

AudioProcessor const *
AudioProcessor::from_id (string id)
{
	for (list<AudioProcessor const *>::const_iterator i = _all.begin(); i != _all.end(); ++i) {
		if ((*i)->id() == id) {
			return *i;
		}
	}

	return 0;
}

list<AudioProcessor const *>
AudioProcessor::all ()
{
	return _all;
}
