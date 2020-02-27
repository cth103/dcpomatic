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

#ifndef DCPOMATIC_SPL_H
#define DCPOMATIC_SPL_H

#include "swaroop_spl_entry.h"
#include <dcp/util.h>

class ContentStore;

class SPL
{
public:
	SPL ()
		: _id (dcp::make_uuid())
		, _missing (false)
	{}

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

	void read (boost::filesystem::path path, ContentStore* store);
	void write (boost::filesystem::path path) const;

	std::string id () const {
		return _id;
	}

	boost::optional<boost::filesystem::path> path () const {
		return _path;
	}

	std::string name () const {
		if (!_path) {
			return "";
		}
		return _path->filename().string();
	}

	bool missing () const {
		return _missing;
	}

	boost::optional<int> allowed_shows () const {
		return _allowed_shows;
	}

	bool have_allowed_shows () const {
		return !_allowed_shows || *_allowed_shows > 0;
	}

	void set_allowed_shows (int s) {
		_allowed_shows = s;
	}

	void unset_allowed_shows () {
		_allowed_shows = boost::optional<int>();
	}

	void decrement_allowed_shows () {
		if (_allowed_shows) {
			(*_allowed_shows)--;
		}

	}

private:
	std::string _id;
	mutable boost::optional<boost::filesystem::path> _path;
	std::vector<SPLEntry> _spl;
	/** true if any content was missing when read() was last called on this SPL */
	bool _missing;
	/** number of times left that the player will allow this playlist to be played (unset means infinite shows) */
	boost::optional<int> _allowed_shows;
};

#endif
