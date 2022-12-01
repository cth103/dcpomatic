/*
    Copyright (C) 2018-2020 Carl Hetherington <cth@carlh.net>

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


#include "spl_entry.h"
#include <dcp/util.h>
#include <boost/signals2.hpp>
#include <algorithm>


class ContentStore;


class SPL
{
public:
	SPL ()
		: _id (dcp::make_uuid())
		, _missing (false)
	{}

	SPL (std::string name)
		: _id (dcp::make_uuid())
		, _name (name)
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
	void swap(size_t a, size_t b) {
		std::iter_swap(_spl.begin() + a, _spl.begin() + b);
	}

	void read (boost::filesystem::path path, ContentStore* store);
	void write (boost::filesystem::path path) const;

	std::string id () const {
		return _id;
	}

	std::string name () const {
		return _name;
	}

	void set_name (std::string name) {
		_name = name;
	}

	bool missing () const {
		return _missing;
	}

private:
	std::string _id;
	std::string _name;
	std::vector<SPLEntry> _spl;
	/** true if any content was missing when read() was last called on this SPL */
	bool _missing;
};


class SignalSPL : public SPL
{
public:
	enum class Change {
		NAME,
		CONTENT,
	};

	SignalSPL () {}

	SignalSPL (std::string name)
		: SPL (name)
	{}

	void set_name (std::string name) {
		SPL::set_name (name);
		Changed(Change::NAME);
	}

	boost::signals2::signal<void (Change)> Changed;
};

#endif
