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


#ifndef DCPOMATIC_FONT_DATA_H
#define DCPOMATIC_FONT_DATA_H


#include <dcp/array_data.h>
#include <boost/optional.hpp>
#include <string>


namespace dcpomatic {


class Font;


/** A font (TTF) file held as a block of data */
class FontData
{
public:
	FontData (std::shared_ptr<const Font> font);

	FontData (std::string id_, dcp::ArrayData data_)
		: id(id_)
		, data(data_)
	{}

	std::string id;
	boost::optional<dcp::ArrayData> data;
};


extern bool operator== (FontData const& a, FontData const& b);
extern bool operator!= (FontData const& a, FontData const& b);


}


#endif
