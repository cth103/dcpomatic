/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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

#include <boost/weak_ptr.hpp>

class Content;
class Film;

class ContentPart
{
public:
	ContentPart (Content* parent, boost::shared_ptr<const Film> film)
		: _parent (parent)
		, _film (film)
	{}

protected:
	Content* _parent;
	boost::weak_ptr<const Film> _film;
	mutable boost::mutex _mutex;
};
