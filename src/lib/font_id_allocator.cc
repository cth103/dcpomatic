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
		add_font(reel_index, asset->id(), font.first);
	}
}


void
FontIDAllocator::add_font(int reel_index, string asset_id, string font_id)
{
	auto font = Font(reel_index, asset_id, font_id);
	if (!_default_font) {
		_default_font = font;
	}
	_map[font] = {};
}


void
FontIDAllocator::allocate()
{
	std::set<string> used_ids;

	for (auto& font: _map) {
		auto proposed = font.first.font_id;
		int prefix = 0;
		while (used_ids.find(proposed) != used_ids.end()) {
			proposed = String::compose("%1_%2", prefix++, font.first.font_id);
			DCPOMATIC_ASSERT(prefix < 128);
		}
		font.second = proposed;
		used_ids.insert(proposed);
	}
}


string
FontIDAllocator::font_id(int reel_index, string asset_id, string font_id) const
{
	auto iter = _map.find(Font(reel_index, asset_id, font_id));
	if (iter == _map.end()) {
		return default_font_id();
	}
	return iter->second;
}


string
FontIDAllocator::default_font_id() const
{
	if (_default_font) {
		return font_id(_default_font->reel_index, _default_font->asset_id, _default_font->font_id);
	}

	return "default";
}
