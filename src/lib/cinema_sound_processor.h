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

/** @file src/cinema_sound_processor.h
 *  @brief CinemaSoundProcessor class
 */

#ifndef DCPOMATIC_CINEMA_SOUND_PROCESSOR_H
#define DCPOMATIC_CINEMA_SOUND_PROCESSOR_H

#include <boost/utility.hpp>
#include <string>
#include <vector>

/** @class CinemaSoundProcessor
 *  @brief Class to describe a cimema's sound processor.
 *
 *  In other words, the box in the rack that handles sound decoding and processing
 *  in a cinema.
 */
class CinemaSoundProcessor : public boost::noncopyable
{
public:
	CinemaSoundProcessor (std::string i, std::string n);

	virtual float db_for_fader_change (float from, float to) const = 0;

	/** @return id for our use */
	std::string id () const {
		return _id;
	}

	/** @return user-visible name for this sound processor */
	std::string name () const {
		return _name;
	}
	
	static std::vector<CinemaSoundProcessor const *> all ();
	static void setup_cinema_sound_processors ();
	static CinemaSoundProcessor const * from_id (std::string id);
	static CinemaSoundProcessor const * from_index (int);
	static int as_index (CinemaSoundProcessor const *);

private:
	/** id for our use */
	std::string _id;
	/** user-visible name for this sound processor */
	std::string _name;

	/** sll available cinema sound processors */
	static std::vector<CinemaSoundProcessor const *> _cinema_sound_processors;
};

#endif
