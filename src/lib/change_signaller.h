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

#ifndef DCPOMATIC_CHANGE_H
#define DCPOMATIC_CHANGE_H

#include <boost/noncopyable.hpp>

template <class T>
class ChangeSignaller : public boost::noncopyable
{
public:
	ChangeSignaller (T* t, int p)
		: _thing (t)
		, _property (p)
		, _done (true)
	{
		_thing->signal_change (CHANGE_TYPE_PENDING, _property);
	}

	~ChangeSignaller ()
	{
		if (_done) {
			_thing->signal_change (CHANGE_TYPE_DONE, _property);
		} else {
			_thing->signal_change (CHANGE_TYPE_CANCELLED, _property);
		}
	}

	void abort ()
	{
		_done = false;
	}

private:
	T* _thing;
	int _property;
	bool _done;
};

#endif
