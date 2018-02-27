/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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

#include "font_files.h"
#include <libcxml/cxml.h>
#include <boost/optional.hpp>
#include <boost/signals2.hpp>
#include <boost/filesystem.hpp>
#include <string>

class Font
{
public:
	explicit Font (std::string id)
		: _id (id) {}

	explicit Font (cxml::NodePtr node);

	void as_xml (xmlpp::Node* node);

	std::string id () const {
		return _id;
	}

	boost::optional<boost::filesystem::path> file (FontFiles::Variant variant) const {
		return _files.get (variant);
	}

	void set_file (FontFiles::Variant variant, boost::filesystem::path file) {
		_files.set (variant, file);
		Changed ();
	}

	FontFiles files () const {
		return _files;
	}

	void set_files (FontFiles files) {
		_files = files;
		Changed ();
	}

	boost::signals2::signal<void()> Changed;

private:
	/** Font ID, used to describe it in the subtitle content */
	std::string _id;
	FontFiles _files;
};

bool operator!= (Font const & a, Font const & b);
bool operator== (Font const & a, Font const & b);

#endif
