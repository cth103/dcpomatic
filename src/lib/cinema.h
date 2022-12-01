/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

/** @file  src/lib/cinema.h
 *  @brief Cinema class.
 */


#include <libcxml/cxml.h>
#include <memory>


namespace xmlpp {
	class Element;
}

namespace dcpomatic {
	class Screen;
}

/** @class Cinema
 *  @brief A description of a Cinema for KDM generation.
 *
 *  This is a cinema name, some metadata and a list of
 *  Screen objects.
 */
class Cinema : public std::enable_shared_from_this<Cinema>
{
public:
	Cinema(std::string const & name_, std::list<std::string> const & e, std::string notes_)
		: name (name_)
		, emails (e)
		, notes (notes_)
	{}

	explicit Cinema (cxml::ConstNodePtr);

	void read_screens (cxml::ConstNodePtr);

	void as_xml (xmlpp::Element *) const;

	void add_screen (std::shared_ptr<dcpomatic::Screen>);
	void remove_screen (std::shared_ptr<dcpomatic::Screen>);

	std::string name;
	std::list<std::string> emails;
	std::string notes;

	std::list<std::shared_ptr<dcpomatic::Screen>> screens () const {
		return _screens;
	}

private:
	std::list<std::shared_ptr<dcpomatic::Screen>> _screens;
};
