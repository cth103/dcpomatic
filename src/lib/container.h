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

#include <vector>
#include <libdcp/util.h>

class Container
{
public:
	Container (libdcp::Size dcp, std::string id, std::string n, std::string d)
		: _dcp_size (dcp)
		, _id (id)
		, _nickname (n)
		, _dci_name (d)
	{}

	/** @return size in pixels of the images that we should
	 *  put in a DCP for this ratio.  This size will not correspond
	 *  to the ratio when we are doing things like 16:9 in a Flat frame.
	 */
	libdcp::Size dcp_size () const {
		return _dcp_size;
	}

	std::string id () const {
		return _id;
	}

	/** @return Full name to present to the user */
	std::string name () const;

	/** @return Nickname (e.g. Flat, Scope) */
	std::string nickname () const {
		return _nickname;
	}

	std::string dci_name () const {
		return _dci_name;
	}

	float ratio () const;
	
	static void setup_containers ();
	static Container const * from_id (std::string i);
	static std::vector<Container const *> all () {
		return _containers;
	}

private:
	/** libdcp::Size in pixels of the images that we should
	 *  put in a DCP for this container.
	 */
	libdcp::Size _dcp_size;
	/** id for use in metadata */
	std::string _id;
	/** nickname (e.g. Flat, Scope) */
	std::string _nickname;
	std::string _dci_name;

	/** all available containers */
	static std::vector<Container const *> _containers;
};
