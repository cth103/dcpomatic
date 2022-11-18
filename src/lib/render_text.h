/*
    Copyright (C) 2014-2018 Carl Hetherington <cth@carlh.net>

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


#include "position_image.h"
#include "dcpomatic_time.h"
#include "string_text.h"
#include <dcp/util.h>
#include <memory>


namespace dcpomatic {
	class Font;
}


std::string marked_up (std::list<StringText> subtitles, int target_height, float fade_factor, std::string font_name);
std::list<PositionImage> render_text (std::list<StringText>, dcp::Size, dcpomatic::DCPTime, int);


class FontMetrics
{
public:
	FontMetrics(int target_height)
		: _target_height(target_height)
	{}

	float baseline_to_bottom(StringText const& subtitle);
	float height(StringText const& subtitle);

private:
	/** Class to collect the properties of a subtitle which affect the metrics we care about
	 *  i.e. baseline position and height.
	 */
	class Identifier
	{
	public:
		Identifier(StringText const& subtitle);

		std::shared_ptr<dcpomatic::Font> font;
		int size;
		float aspect_adjust;

		bool operator<(Identifier const& other) const;
	};

	using Cache = std::map<Identifier, std::pair<float, float>>;

	Cache::iterator get(StringText const& subtitle);

	Cache _cache;

	int _target_height;
};

