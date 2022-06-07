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


#ifndef DCPOMATIC_PLAYER_CAPTION_H
#define DCPOMATIC_PLAYER_CAPTION_H


#include "bitmap_text.h"
#include "string_text.h"


namespace dcpomatic {
	class Font;
}


/** A set of text (subtitle/CCAP) which span the same time period */
class PlayerText
{
public:
	void add_fonts (std::list<std::shared_ptr<dcpomatic::Font>> fonts_);
	std::list<std::shared_ptr<dcpomatic::Font>> fonts;

	/** BitmapTexts, with their rectangles transformed as specified by their content */
	std::list<BitmapText> bitmap;
	std::list<StringText> string;
};


#endif
