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

/** @file src/filter.h
 *  @brief A class to describe one of FFmpeg's video or post-processing filters.
 */

#ifndef DCPOMATIC_FILTER_H
#define DCPOMATIC_FILTER_H

#include <string>
#include <vector>

/** @class Filter
 *  @brief A class to describe one of FFmpeg's video or post-processing filters.
 */
class Filter
{
public:
	Filter (std::string, std::string, std::string, std::string, std::string);

	/** @return our id */
	std::string id () const {
		return _id;
	}

	/** @return user-visible name */
	std::string name () const {
		return _name;
	}

	/** @return string for a FFmpeg video filter descriptor */
	std::string vf () const {
		return _vf;
	}
	
	/** @return string for a FFmpeg post-processing descriptor */
	std::string pp () const {
		return _pp;
	}

	std::string category () const {
		return _category;
	}
	
	static std::vector<Filter const *> all ();
	static Filter const * from_id (std::string);
	static void setup_filters ();
	static std::pair<std::string, std::string> ffmpeg_strings (std::vector<Filter const *> const &);

private:

	/** our id */
	std::string _id;
	/** user-visible name */
	std::string _name;
	std::string _category;
	/** string for a FFmpeg video filter descriptor */
	std::string _vf;
	/** string for a FFmpeg post-processing descriptor */
	std::string _pp;

	/** all available filters */
	static std::vector<Filter const *> _filters;
	static void maybe_add (std::string, std::string, std::string, std::string, std::string);
};

#endif
