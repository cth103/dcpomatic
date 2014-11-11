/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_RATIO_H
#define DCPOMATIC_RATIO_H

#include <vector>
#include <boost/utility.hpp>
#include <dcp/util.h>

class Ratio : public boost::noncopyable
{
public:
	Ratio (float ratio, std::string id, std::string n, std::string d)
		: _ratio (ratio)
		, _id (id)
		, _nickname (n)
		, _isdcf_name (d)
	{}

	std::string id () const {
		return _id;
	}

	std::string nickname () const {
		return _nickname;
	}

	std::string isdcf_name () const {
		return _isdcf_name;
	}

	float ratio () const {
		return _ratio;
	}

	static void setup_ratios ();
	static Ratio const * from_id (std::string i);
	static Ratio const * from_ratio (float r);
	static Ratio const * nearest_from_ratio (float r);
	static std::vector<Ratio const *> all () {
		return _ratios;
	}

private:
	float _ratio;
	/** id for use in metadata */
	std::string _id;
	/** nickname (e.g. Flat, Scope) */
	std::string _nickname;
	std::string _isdcf_name;

	static std::vector<Ratio const *> _ratios;	
};

#endif
