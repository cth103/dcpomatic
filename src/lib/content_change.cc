/*
    Copyright (C) 2013-2018 Carl Hetherington <cth@carlh.net>

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

#include "content_change.h"
#include "content.h"

ContentChange::ContentChange (Content* c, int p)
	: _content (c)
	, _property (p)
	, _done (true)
{
	_content->signal_change (CHANGE_TYPE_PENDING, _property);
}


ContentChange::~ContentChange ()
{
	if (_done) {
		_content->signal_change (CHANGE_TYPE_DONE, _property);
	} else {
		_content->signal_change (CHANGE_TYPE_CANCELLED, _property);
	}
}

void
ContentChange::abort ()
{
	_done = false;
}
