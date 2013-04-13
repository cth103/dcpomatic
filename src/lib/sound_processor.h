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

/** @file src/sound_processor.h
 *  @brief A class to describe a sound processor.
 */

#ifndef DCPOMATIC_SOUND_PROCESSOR_H
#define DCPOMATIC_SOUND_PROCESSOR_H

#include <string>
#include <vector>

/** @class SoundProcessor
 *  @brief Class to describe a sound processor.
 */
class SoundProcessor
{
public:
	SoundProcessor (std::string i, std::string n);

	virtual float db_for_fader_change (float from, float to) const = 0;

	/** @return id for our use */
	std::string id () const {
		return _id;
	}

	/** @return user-visible name for this sound processor */
	std::string name () const {
		return _name;
	}
	
	static std::vector<SoundProcessor const *> all ();
	static void setup_sound_processors ();
	static SoundProcessor const * from_id (std::string id);
	static SoundProcessor const * from_index (int);
	static int as_index (SoundProcessor const *);

private:
	/** id for our use */
	std::string _id;
	/** user-visible name for this sound processor */
	std::string _name;

	/** sll available sound processors */
	static std::vector<SoundProcessor const *> _sound_processors;
};

#endif
