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


#ifndef DCPOMATIC_SHUFFLER_H
#define DCPOMATIC_SHUFFLER_H


#include "content_video.h"
#include "types.h"
#include <boost/signals2.hpp>


struct shuffler_test5;

class Piece;


class Shuffler
{
public:
	void clear ();
	void flush ();
	void video (std::weak_ptr<Piece>, ContentVideo video);

	boost::signals2::signal<void (std::weak_ptr<Piece>, ContentVideo)> Video;

	typedef std::pair<std::weak_ptr<Piece>, ContentVideo> Store;

private:
	friend struct ::shuffler_test5;

	std::list<Store> _store;
	boost::optional<ContentVideo> _last;
	static int const _max_size;
};


#endif

