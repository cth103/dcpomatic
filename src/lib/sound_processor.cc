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

/** @file src/sound_processor.cc
 *  @brief A class to describe a sound processor.
 */

#include <iostream>
#include <cassert>
#include "sound_processor.h"
#include "dolby_cp750.h"

using namespace std;

vector<SoundProcessor const *> SoundProcessor::_sound_processors;

/** @param i Our id.
 *  @param n User-visible name.
 */
SoundProcessor::SoundProcessor (string i, string n)
	: _id (i)
	, _name (n)
{

}

/** @return All available sound processors */
vector<SoundProcessor const *>
SoundProcessor::all ()
{
	return _sound_processors;
}

/** Set up the static _sound_processors vector; must be called before from_*
 *  methods are used.
 */
void
SoundProcessor::setup_sound_processors ()
{
	_sound_processors.push_back (new DolbyCP750);
}

/** @param id One of our ids.
 *  @return Corresponding sound processor, or 0.
 */
SoundProcessor const *
SoundProcessor::from_id (string id)
{
	vector<SoundProcessor const *>::iterator i = _sound_processors.begin ();
	while (i != _sound_processors.end() && (*i)->id() != id) {
		++i;
	}

	if (i == _sound_processors.end ()) {
		return 0;
	}

	return *i;
}

/** @param s A sound processor from our static list.
 *  @return Index of the sound processor with the list, or -1.
 */
int
SoundProcessor::as_index (SoundProcessor const * s)
{
	vector<SoundProcessor*>::size_type i = 0;
	while (i < _sound_processors.size() && _sound_processors[i] != s) {
		++i;
	}

	if (i == _sound_processors.size ()) {
		return -1;
	}

	return i;
}

/** @param i An index returned from as_index().
 *  @return Corresponding sound processor.
 */
SoundProcessor const *
SoundProcessor::from_index (int i)
{
	assert (i <= int(_sound_processors.size ()));
	return _sound_processors[i];
}
