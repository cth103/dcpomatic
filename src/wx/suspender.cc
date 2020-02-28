/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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

#include "suspender.h"
#include <boost/foreach.hpp>

Suspender::Suspender(boost::function<void (int)> handler)
	: _handler (handler)
	, _count (0)
{

}

Suspender::Block::Block (Suspender* s)
	: _suspender (s)
{
	_suspender->increment ();
}

Suspender::Block::~Block ()
{
	_suspender->decrement ();
}

Suspender::Block
Suspender::block ()
{
	return Block (this);
}

void
Suspender::increment ()
{
	++_count;
}

void
Suspender::decrement ()
{
	--_count;
	if (_count == 0) {
		BOOST_FOREACH (int i, _pending) {
			_handler (i);
		}
		_pending.clear ();
	}
}

bool
Suspender::check (int property)
{
	if (_count == 0) {
		return false;
	}

	_pending.insert (property);
	return true;
}


