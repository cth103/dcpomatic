/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


/** @file src/cinema_sound_processor.h
 *  @brief CinemaSoundProcessor class
 */


#ifndef DCPOMATIC_CINEMA_SOUND_PROCESSOR_H
#define DCPOMATIC_CINEMA_SOUND_PROCESSOR_H


#include <boost/utility.hpp>
#include <memory>
#include <string>
#include <vector>


/** @class CinemaSoundProcessor
 *  @brief Class to describe a cimema's sound processor.
 *
 *  In other words, the box in the rack that handles sound decoding and processing
 *  in a cinema.
 */
class CinemaSoundProcessor
{
public:
	CinemaSoundProcessor (std::string i, std::string n, float knee, float below, float above);
	virtual ~CinemaSoundProcessor () {}

	CinemaSoundProcessor (CinemaSoundProcessor const&) = delete;
	CinemaSoundProcessor& operator=(CinemaSoundProcessor const&) = delete;

	float db_for_fader_change (float from, float to) const;

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

private:
	/** id for our use */
	std::string _id;
	/** user-visible name for this sound processor */
	std::string _name;
	float _knee;
	float _below;
	float _above;

	/** all available cinema sound processors */
	static std::vector<std::unique_ptr<const CinemaSoundProcessor>> _cinema_sound_processors;
};


#endif
