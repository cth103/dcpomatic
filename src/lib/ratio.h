/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_RATIO_H
#define DCPOMATIC_RATIO_H


#include <dcp/util.h>
#include <boost/utility.hpp>
#include <vector>


/** @class Ratio
 *  @brief Description of an image ratio.
 */
class Ratio
{
public:
	Ratio (float ratio, std::string id, std::string in, boost::optional<std::string> cn, std::string d)
		: _ratio (ratio)
		, _id (id)
		, _image_nickname (in)
		, _container_nickname (cn)
		, _isdcf_name (d)
	{}

	Ratio (Ratio const&) = delete;
	Ratio& operator= (Ratio const&) = delete;

	std::string id () const {
		return _id;
	}

	std::string image_nickname () const {
		return _image_nickname;
	}

	std::string container_nickname () const;

	bool used_for_container () const {
		return static_cast<bool>(_container_nickname);
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

	static std::vector<Ratio const *> containers ();

private:
	float _ratio;
	/** id for use in metadata */
	std::string _id;
	/** nickname when used to describe an image ratio (e.g. Flat, Scope) */
	std::string _image_nickname;
	/** nickname when used to describe a container ratio */
	boost::optional<std::string> _container_nickname;
	std::string _isdcf_name;

	static std::vector<Ratio const *> _ratios;
};


#endif
