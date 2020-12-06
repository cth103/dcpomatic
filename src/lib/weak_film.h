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


#ifndef DCPOMATIC_WEAK_FILM_H
#define DCPOMATIC_WEAK_FILM_H


#include "dcpomatic_assert.h"
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>


class Film;


template <class T>
class WeakFilmTemplate
{
public:
	WeakFilmTemplate (boost::weak_ptr<T> f)
		: _film(f)
	{}

protected:
	boost::weak_ptr<T> _film;

	boost::shared_ptr<T> film () const {
		boost::shared_ptr<T> f = _film.lock();
		DCPOMATIC_ASSERT (f);
		return f;
	}
};


typedef WeakFilmTemplate<const Film> WeakConstFilm;
typedef WeakFilmTemplate<Film> WeakFilm;


#endif
