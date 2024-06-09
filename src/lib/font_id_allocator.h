/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_FONT_ID_ALLOCATOR_H
#define DCPOMATIC_FONT_ID_ALLOCATOR_H


#include <map>
#include <memory>
#include <string>
#include <vector>
#include <boost/optional.hpp>


namespace dcp {
	class Reel;
	class SubtitleAsset;
}


/** A class which, given some pairs of (asset-id, font-id) can return a font ID
 *  which is unique in a piece of content.
 *
 *  When we examine a 2-reel DCP we may have a pair of subtitle assets that
 *  each have a font with ID "foo".  We want to store these in
 *  TextContent::_fonts in such a way that they are distinguishable.
 *
 *  If TextContent is to carry on storing a list of dcpomatic::Font, we can
 *  only do this by making each dcpomatic::Font have a different ID (i.e. not
 *  both "foo").
 *
 *  Hence when we add the fonts to the TextContent we re-write them to have
 *  unique IDs.
 *
 *  When we do this, we must do it in a repeatable way so that when the
 *  DCPDecoder receives the "foo" font IDs it can obtain the same "new" ID given
 *  "foo" and the asset ID that it came from.
 *
 *  FontIDAllocator can help with that: call add_fonts_from_reels() or add_font(),
 *  then allocate(), then it will return repeatable unique "new" font IDs from
 *  an asset map and "old" ID.
 */
class FontIDAllocator
{
public:
	void add_fonts_from_reels(std::vector<std::shared_ptr<dcp::Reel>> const& reels);
	void add_font(int reel_index, std::string asset_id, std::string font_id);

	void allocate();

	std::string font_id(int reel_index, std::string asset_id, std::string font_id) const;
	std::string default_font_id() const;

	bool has_default_font() const {
		return static_cast<bool>(_default_font);
	}

private:
	void add_fonts_from_asset(int reel_index, std::shared_ptr<const dcp::SubtitleAsset> asset);

	struct Font
	{
		Font(int reel_index_, std::string asset_id_, std::string font_id_)
			: reel_index(reel_index_)
			, asset_id(asset_id_)
			, font_id(font_id_)
		{}

		bool operator<(Font const& other) const {
			if (reel_index != other.reel_index) {
				return reel_index < other.reel_index;
			}

			if (asset_id != other.asset_id) {
				return asset_id < other.asset_id;
			}

			return font_id < other.font_id;
		}

		int reel_index;
		std::string asset_id;
		std::string font_id;
	};

	std::map<Font, std::string> _map;
	boost::optional<Font> _default_font;
};


#endif
