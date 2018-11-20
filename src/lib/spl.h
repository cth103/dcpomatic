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

#include "spl_entry.h"

class ContentStore;

class SPL
{
public:
	void add (SPLEntry e) {
		_spl.push_back (e);
	}

	void remove (std::size_t index) {
		_spl.erase (_spl.begin() + index);
	}

	std::vector<SPLEntry> const & get () const {
		return _spl;
	}

	SPLEntry & operator[] (std::size_t index) {
		return _spl[index];
	}

	SPLEntry const & operator[] (std::size_t index) const {
		return _spl[index];
	}

	bool read (boost::filesystem::path path, ContentStore* store);
	void write (boost::filesystem::path path) const;

	std::string name () const {
		return _name;
	}

private:
	std::string _name;
	std::vector<SPLEntry> _spl;
};
