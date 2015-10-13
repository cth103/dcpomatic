/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

/** @return Pieces of content type C that overlap a specified time range in the given ContentList */
template<class C>
std::list<boost::shared_ptr<C> >
overlaps (ContentList cl, DCPTime from, DCPTime to)
{
	std::list<boost::shared_ptr<C> > overlaps;
	DCPTimePeriod period (from, to);
	for (typename ContentList::const_iterator i = cl.begin(); i != cl.end(); ++i) {
		boost::shared_ptr<C> c = boost::dynamic_pointer_cast<C> (*i);
		if (!c) {
			continue;
		}

		if (DCPTimePeriod(c->position(), c->end()).overlaps (period)) {
			overlaps.push_back (c);
		}
	}

	return overlaps;
}
