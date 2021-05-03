/*
    Copyright (C) 2016-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_CONTENT_PART_H
#define DCPOMATIC_CONTENT_PART_H


#include "change_signaller.h"
#include "content.h"
#include <boost/thread/mutex.hpp>


class Content;
class Film;


class ContentPart
{
public:
	explicit ContentPart (Content* parent)
		: _parent (parent)
	{}

protected:
	template <class T>
	void
	maybe_set (T& member, T new_value, int property) const
	{
		ContentChangeSignaller cc (_parent, property);
		{
			boost::mutex::scoped_lock lm (_mutex);
			if (member == new_value) {
				cc.abort ();
				return;
			}
			member = new_value;
		}
	}

	template <class T>
	void
	maybe_set (boost::optional<T>& member, T new_value, int property) const
	{
		ContentChangeSignaller cc (_parent, property);
		{
			boost::mutex::scoped_lock lm (_mutex);
			if (member && member.get() == new_value) {
				cc.abort ();
				return;
			}
			member = new_value;
		}
	}

	Content* _parent;
	mutable boost::mutex _mutex;
};


#endif
