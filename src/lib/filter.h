/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

/** @file src/filter.h
 *  @brief A class to describe one of FFmpeg's video or audio filters.
 */

#ifndef DCPOMATIC_FILTER_H
#define DCPOMATIC_FILTER_H

#include <boost/utility.hpp>
#include <string>
#include <vector>

/** @class Filter
 *  @brief A class to describe one of FFmpeg's video or audio filters.
 *
 *  We don't support FFmpeg's post-processing filters here as they cannot cope with greater than
 *  8bpp.  FFmpeg quantizes e.g. yuv422p10le down to yuv422p before running such filters, which
 *  we don't really want to do.
 */
class Filter : public boost::noncopyable
{
public:
	Filter (std::string i, std::string n, std::string c, std::string f);

	/** @return our id */
	std::string id () const {
		return _id;
	}

	/** @return user-visible name */
	std::string name () const {
		return _name;
	}

	/** @return string for a FFmpeg filter descriptor */
	std::string ffmpeg () const {
		return _ffmpeg;
	}

	std::string category () const {
		return _category;
	}

	static std::vector<Filter const *> all ();
	static Filter const * from_id (std::string d);
	static void setup_filters ();
	static std::string ffmpeg_string (std::vector<Filter const *> const & filters);

private:

	/** our id */
	std::string _id;
	/** user-visible name */
	std::string _name;
	std::string _category;
	/** string for a FFmpeg filter descriptor */
	std::string _ffmpeg;

	/** all available filters */
	static std::vector<Filter const *> _filters;
	static void maybe_add (std::string, std::string, std::string, std::string);
};

#endif
