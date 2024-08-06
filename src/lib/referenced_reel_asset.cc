/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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


#include "dcp_content.h"
#include "dcp_decoder.h"
#include "dcpomatic_assert.h"
#include "film.h"
#include "playlist.h"
#include "referenced_reel_asset.h"
#include <dcp/reel.h>
#include <dcp/reel_asset.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/reel_text_asset.h>
#include <cmath>


using std::list;
using std::max;
using std::min;
using std::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::scoped_ptr;
using namespace dcpomatic;


static void
maybe_add_asset (list<ReferencedReelAsset>& a, shared_ptr<dcp::ReelAsset> r, Frame reel_trim_start, Frame reel_trim_end, DCPTime from, int const ffr)
{
	DCPOMATIC_ASSERT (r);
	r->set_entry_point (r->entry_point().get_value_or(0) + reel_trim_start);
	r->set_duration (r->actual_duration() - reel_trim_start - reel_trim_end);
	if (r->actual_duration() > 0) {
		a.push_back (
			ReferencedReelAsset(r, DCPTimePeriod(from, from + DCPTime::from_frames(r->actual_duration(), ffr)))
			);
	}
}



/** @return Details of all the DCP assets in a playlist that are marked to refer to */
list<ReferencedReelAsset>
get_referenced_reel_assets(shared_ptr<const Film> film, shared_ptr<const Playlist> playlist)
{
	list<ReferencedReelAsset> reel_assets;

	for (auto content: playlist->content()) {
		auto dcp = dynamic_pointer_cast<DCPContent>(content);
		if (!dcp) {
			continue;
		}

		if (!dcp->reference_video() && !dcp->reference_audio() && !dcp->reference_text(TextType::OPEN_SUBTITLE) && !dcp->reference_text(TextType::CLOSED_CAPTION)) {
			continue;
		}

		scoped_ptr<DCPDecoder> decoder;
		try {
			decoder.reset(new DCPDecoder(film, dcp, false, false, shared_ptr<DCPDecoder>()));
		} catch (...) {
			return reel_assets;
		}

		auto const frame_rate = film->video_frame_rate();
		DCPOMATIC_ASSERT (dcp->video_frame_rate());
		/* We should only be referencing if the DCP rate is the same as the film rate */
		DCPOMATIC_ASSERT (std::round(dcp->video_frame_rate().get()) == frame_rate);

		Frame const trim_start = dcp->trim_start().frames_round(frame_rate);
		Frame const trim_end = dcp->trim_end().frames_round(frame_rate);

		/* position in the asset from the start */
		int64_t offset_from_start = 0;
		/* position i the asset from the end */
		int64_t offset_from_end = 0;
		for (auto reel: decoder->reels()) {
			/* Assume that main picture duration is the length of the reel */
			offset_from_end += reel->main_picture()->actual_duration();
		}

		for (auto reel: decoder->reels()) {

			/* Assume that main picture duration is the length of the reel */
			int64_t const reel_duration = reel->main_picture()->actual_duration();

			/* See doc/design/trim_reels.svg */
			Frame const reel_trim_start = min(reel_duration, max(int64_t(0), trim_start - offset_from_start));
			Frame const reel_trim_end =   min(reel_duration, max(int64_t(0), reel_duration - (offset_from_end - trim_end)));

			auto const from = content->position() + std::max(DCPTime(), DCPTime::from_frames(offset_from_start - trim_start, frame_rate));
			if (dcp->reference_video()) {
				maybe_add_asset (reel_assets, reel->main_picture(), reel_trim_start, reel_trim_end, from, frame_rate);
			}

			if (dcp->reference_audio()) {
				maybe_add_asset (reel_assets, reel->main_sound(), reel_trim_start, reel_trim_end, from, frame_rate);
			}

			if (dcp->reference_text(TextType::OPEN_SUBTITLE) && reel->main_subtitle()) {
				maybe_add_asset (reel_assets, reel->main_subtitle(), reel_trim_start, reel_trim_end, from, frame_rate);
			}

			if (dcp->reference_text(TextType::CLOSED_CAPTION)) {
				for (auto caption: reel->closed_captions()) {
					maybe_add_asset (reel_assets, caption, reel_trim_start, reel_trim_end, from, frame_rate);
				}
			}

			offset_from_start += reel_duration;
			offset_from_end -= reel_duration;
		}
	}

	return reel_assets;
}

