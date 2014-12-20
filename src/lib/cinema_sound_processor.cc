/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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
 *  @brief CinemaSoundProcessor class.
 */

#include "cinema_sound_processor.h"
#include "dolby_cp750.h"
#include "dcpomatic_assert.h"
#include <iostream>
#include <cassert>

using namespace std;

vector<CinemaSoundProcessor const *> CinemaSoundProcessor::_cinema_sound_processors;

/** @param i Our id.
 *  @param n User-visible name.
 */
CinemaSoundProcessor::CinemaSoundProcessor (string i, string n)
	: _id (i)
	, _name (n)
{

}

/** @return All available sound processors */
vector<CinemaSoundProcessor const *>
CinemaSoundProcessor::all ()
{
	return _cinema_sound_processors;
}

/** Set up the static _sound_processors vector; must be called before from_*
 *  methods are used.
 */
void
CinemaSoundProcessor::setup_cinema_sound_processors ()
{
	_cinema_sound_processors.push_back (new DolbyCP750);
}

/** @param id One of our ids.
 *  @return Corresponding sound processor, or 0.
 */
CinemaSoundProcessor const *
CinemaSoundProcessor::from_id (string id)
{
	vector<CinemaSoundProcessor const *>::iterator i = _cinema_sound_processors.begin ();
	while (i != _cinema_sound_processors.end() && (*i)->id() != id) {
		++i;
	}

	if (i == _cinema_sound_processors.end ()) {
		return 0;
	}

	return *i;
}

/** @param s A sound processor from our static list.
 *  @return Index of the sound processor with the list, or -1.
 */
int
CinemaSoundProcessor::as_index (CinemaSoundProcessor const * s)
{
	vector<CinemaSoundProcessor*>::size_type i = 0;
	while (i < _cinema_sound_processors.size() && _cinema_sound_processors[i] != s) {
		++i;
	}

	if (i == _cinema_sound_processors.size ()) {
		return -1;
	}

	return i;
}

/** @param i An index returned from as_index().
 *  @return Corresponding sound processor.
 */
CinemaSoundProcessor const *
CinemaSoundProcessor::from_index (int i)
{
	DCPOMATIC_ASSERT (i <= int(_cinema_sound_processors.size ()));
	return _cinema_sound_processors[i];
}
