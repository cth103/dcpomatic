/*
    Copyright (C) 2020-2021 Carl Hetherington <cth@carlh.net>

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
#include "font_data.h"
#include "dcpomatic_assert.h"


using std::shared_ptr;


dcpomatic::FontData::FontData (shared_ptr<const Font> font)
	: id (font->id())
{
	if (font->file()) {
		data = dcp::ArrayData(font->file().get());
	}
}


bool
dcpomatic::operator== (FontData const& a, FontData const& b)
{
	if (a.id != b.id) {
		return false;
	}

	return a.data == b.data;
}


bool
dcpomatic::operator!= (FontData const& a, FontData const& b)
{
	return !(a == b);
}

