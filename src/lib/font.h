/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_FONT_H
#define DCPOMATIC_FONT_H


#include <libcxml/cxml.h>
#include <boost/optional.hpp>
#include <boost/signals2.hpp>
#include <boost/filesystem.hpp>
#include <string>


namespace dcpomatic {


class Font
{
public:
	explicit Font (std::string id)
		: _id (id) {}

	explicit Font (cxml::NodePtr node);

	Font (std::string id, boost::filesystem::path file)
		: _id (id)
		, _file (file)
	{}

	void as_xml (xmlpp::Node* node);

	std::string id () const {
		return _id;
	}

	boost::optional<boost::filesystem::path> file () const {
		return _file;
	}

	void set_file (boost::filesystem::path file) {
		_file = file;
		Changed ();
	}

	boost::signals2::signal<void()> Changed;

private:
	/** Font ID, used to describe it in the subtitle content */
	std::string _id;
	boost::optional<boost::filesystem::path> _file;
};


bool operator!= (Font const & a, Font const & b);
bool operator== (Font const & a, Font const & b);


}

#endif
