/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_CHANGE_SIGNALLER_H
#define DCPOMATIC_CHANGE_SIGNALLER_H


enum class ChangeType
{
	PENDING,
	DONE,
	CANCELLED
};


template <class T, class P>
class ChangeSignaller
{
public:
	ChangeSignaller (T* t, P p)
		: _thing (t)
		, _property (p)
		, _done (true)
	{
		_thing->signal_change (ChangeType::PENDING, _property);
	}

	~ChangeSignaller ()
	{
		if (_done) {
			_thing->signal_change (ChangeType::DONE, _property);
		} else {
			_thing->signal_change (ChangeType::CANCELLED, _property);
		}
	}

	ChangeSignaller (ChangeSignaller const&) = delete;
	ChangeSignaller& operator= (ChangeSignaller const&) = delete;

	void abort ()
	{
		_done = false;
	}

private:
	T* _thing;
	P _property;
	bool _done;
};


#endif
