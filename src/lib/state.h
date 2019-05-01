/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_STATE_H
#define DCPOMATIC_STATE_H

#include <boost/noncopyable.hpp>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>

class State : public boost::noncopyable
{
public:
	virtual ~State () {}
	virtual void read () = 0;
	virtual void write () const = 0;

	/** If set, this overrides the standard path (in home, Library, AppData or wherever) for config.xml, cinemas.xml etc. */
	static boost::optional<boost::filesystem::path> override_path;
	static boost::filesystem::path path (std::string file, bool create_directories = true);
};

#endif
