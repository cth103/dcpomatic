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
#include <boost/enable_shared_from_this.hpp>

namespace xmlpp {
	class Element;
}

class Screen;

/** @class Cinema
 *  @brief A description of a Cinema for KDM generation.
 *
 *  This is a cinema name, some metadata and a list of
 *  Screen objects.
 */
class Cinema : public boost::enable_shared_from_this<Cinema>
{
public:
	Cinema (std::string const & name_, std::list<std::string> const & e, std::string notes_, int utc_offset_hour, int utc_offset_minute)
		: name (name_)
		, emails (e)
		, notes (notes_)
		, _utc_offset_hour (utc_offset_hour)
		, _utc_offset_minute (utc_offset_minute)
	{}

	Cinema (cxml::ConstNodePtr);

	void read_screens (cxml::ConstNodePtr);

	void as_xml (xmlpp::Element *) const;

	void add_screen (boost::shared_ptr<Screen>);
	void remove_screen (boost::shared_ptr<Screen>);

	void set_utc_offset_hour (int h);
	void set_utc_offset_minute (int m);

	std::string name;
	std::list<std::string> emails;
	std::string notes;

	int utc_offset_hour () const {
		return _utc_offset_hour;
	}

	int utc_offset_minute () const {
		return _utc_offset_minute;
	}

	std::list<boost::shared_ptr<Screen> > screens () const {
		return _screens;
	}

private:
	std::list<boost::shared_ptr<Screen> > _screens;
	/** Offset such that the equivalent time in UTC can be determined
	    by subtracting the offset from the local time.
	*/
	int _utc_offset_hour;
	/** Additional minutes to add to _utc_offset_hour if _utc_offset_hour is
	    positive, or to subtract if _utc_offset_hour is negative.
	*/
	int _utc_offset_minute;
};
