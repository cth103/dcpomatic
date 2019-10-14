/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include "audio_processor.h"
#include "mid_side_decoder.h"
#include "upmixer_a.h"
#include "upmixer_b.h"
#include "config.h"

using std::string;
using std::list;

list<AudioProcessor const *> AudioProcessor::_all;
list<AudioProcessor const *> AudioProcessor::_non_experimental;

void
AudioProcessor::setup_audio_processors ()
{
	AudioProcessor* mid_side = new MidSideDecoder ();
	_all.push_back (mid_side);
	_non_experimental.push_back (mid_side);

	_all.push_back (new UpmixerA (48000));
	_all.push_back (new UpmixerB (48000));
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
AudioProcessor::visible ()
{
	if (Config::instance()->show_experimental_audio_processors()) {
		return _all;
	}

	return _non_experimental;
}

list<AudioProcessor const *>
AudioProcessor::all ()
{
	return _all;
}
