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


#include "compose.hpp"
#include "constants.h"
#include "dcpomatic_assert.h"
#include "font_id_allocator.h"
#include <dcp/reel.h>
#include <dcp/reel_closed_caption_asset.h>
#include <dcp/reel_subtitle_asset.h>
#include <dcp/subtitle_asset.h>
#include <set>
#include <string>
#include <vector>


using std::shared_ptr;
using std::set;
using std::string;
using std::vector;


void
FontIDAllocator::add_fonts_from_reels(vector<shared_ptr<dcp::Reel>> const& reels)
{
	int reel_index = 0;
	for (auto reel: reels) {
		if (auto sub = reel->main_subtitle()) {
			if (sub->asset_ref().resolved()) {
				add_fonts_from_asset(reel_index, sub->asset());
			}
		}

		for (auto ccap: reel->closed_captions()) {
			if (ccap->asset_ref().resolved()) {
				add_fonts_from_asset(reel_index, ccap->asset());
			}
		}

		++reel_index;
	}
}


void
FontIDAllocator::add_fonts_from_asset(int reel_index, shared_ptr<const dcp::SubtitleAsset> asset)
{
	for (auto const& font: asset->font_data()) {
		_map[Font(reel_index, asset->id(), font.first)] = 0;
	}

	_map[Font(reel_index, asset->id(), "")] = 0;
}


void
FontIDAllocator::add_font(int reel_index, string asset_id, string font_id)
{
	_map[Font(reel_index, asset_id, font_id)] = 0;
}


void
FontIDAllocator::allocate()
{
	/* Last reel index that we have; i.e. the last prefix number that would be used by "old"
	 * font IDs (i.e. ones before this FontIDAllocator was added and used)
	 */
	auto const last_reel = std::max_element(
		_map.begin(),
		_map.end(),
		[] (std::pair<Font, int> const& a, std::pair<Font, int> const& b) {
			return a.first.reel_index < b.first.reel_index;
		})->first.reel_index;

	/* Number of times each ID has been used */
	std::map<string, int> used_count;

	for (auto& font: _map) {
		auto const proposed = String::compose("%1_%2", font.first.reel_index, font.first.font_id);
		if (used_count.find(proposed) != used_count.end()) {
			/* This ID was already used; we need to disambiguate it.  Do so by using
			 * an ID above last_reel
			 */
			font.second = last_reel + used_count[proposed];
			++used_count[proposed];
		} else {
			/* This ID was not yet used */
			used_count[proposed] = 1;
			font.second = font.first.reel_index;
		}
	}
}


string
FontIDAllocator::font_id(int reel_index, string asset_id, string font_id) const
{
	auto iter = _map.find(Font(reel_index, asset_id, font_id));
	DCPOMATIC_ASSERT(iter != _map.end());
	return String::compose("%1_%2", iter->second, font_id);
}

