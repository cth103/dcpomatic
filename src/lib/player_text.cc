/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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


#include "font.h"
#include "player_text.h"


using std::list;
using std::shared_ptr;
using namespace dcpomatic;


void
PlayerText::add_fonts (list<shared_ptr<Font>> fonts_)
{
	for (auto i: fonts_) {
		bool got = false;
		for (auto j: fonts) {
			if (*i == *j) {
				got = true;
			}
		}
		if (!got) {
			fonts.push_back (i);
		}
	}
}
