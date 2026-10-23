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
#include "dcpomatic_log.h"
#include "film.h"
#include "playlist.h"
#include "referenced_reel_asset.h"
#include "sound_mxf_content.h"
#include "video_mxf_content.h"
#include <dcp/filesystem.h>
#include <dcp/mono_j2k_picture_asset.h>
#include <dcp/reel.h>
#include <dcp/reel_asset.h>
#include <dcp/reel_atmos_asset.h>
#include <dcp/reel_mono_picture_asset.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/reel_text_asset.h>
#include <dcp/sound_asset.h>
#include <cmath>


using std::list;
using std::max;
using std::min;
using std::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::scoped_ptr;
using namespace dcpomatic;


static void
maybe_add_asset(list<ReferencedReelAsset>& a, shared_ptr<dcp::ReelAsset> r, Frame reel_trim_start, Frame reel_trim_end, DCPTime from, int const ffr)
{
	DCPOMATIC_ASSERT(r);
	r->set_entry_point(r->entry_point().get_value_or(0) + reel_trim_start);
	r->set_duration(r->actual_duration() - reel_trim_start - reel_trim_end);
	if (r->actual_duration() > 0) {
		a.push_back(
			ReferencedReelAsset(r, DCPTimePeriod(from, from + DCPTime::from_frames(r->actual_duration(), ffr)))
			);
	}
}



/** Put a file that is not in another DCP into this one, as @param in_dcp, a name made from its ID so that two files
 *  with the same name do not collide: linked, or copied if it cannot be linked.  The source may already be the file in
 *  the DCP folder, or a copy of it (from an earlier DCP of this film, or another content using the same file); then
 *  there is nothing to do.  A file there with the same name, which holds the asset's ID, and the same size is taken
 *  to be such a copy.
 */
static void
put_in_dcp(boost::filesystem::path source, boost::filesystem::path in_dcp)
{
	boost::system::error_code ec;
	auto const there = dcp::filesystem::exists(in_dcp, ec);
	auto const same = there && (
		boost::filesystem::equivalent(source, in_dcp, ec) ||
		boost::filesystem::file_size(source, ec) == boost::filesystem::file_size(in_dcp, ec)
		);
	if (!same) {
		dcp::filesystem::remove(in_dcp, ec);
		dcp::filesystem::create_hard_link(source, in_dcp, ec);
		if (ec) {
			dcp::filesystem::copy_file(source, in_dcp);
		}
	}
}


/** @return Details of all the DCP assets in a playlist that are marked to refer to */
list<ReferencedReelAsset>
get_referenced_reel_assets(shared_ptr<const Film> film, shared_ptr<const Playlist> playlist)
{
	list<ReferencedReelAsset> reel_assets;

	for (auto content: playlist->content()) {
		if (auto sound_mxf = dynamic_pointer_cast<SoundMXFContent>(content)) {
			std::string why_not;
			if (sound_mxf->can_reference_audio(film, why_not)) {
				auto const frame_rate = film->video_frame_rate();

				auto const source = sound_mxf->path(0);
				auto const in_dcp = film->dir(film->dcp_name()) / ("pcm_" + dcp::SoundAsset(source).id() + ".mxf");
				put_in_dcp(source, in_dcp);

				maybe_add_asset(
					reel_assets,
					std::make_shared<dcp::ReelSoundAsset>(std::make_shared<dcp::SoundAsset>(in_dcp), 0),
					sound_mxf->trim_start().frames_round(frame_rate),
					sound_mxf->trim_end().frames_round(frame_rate),
					sound_mxf->position(),
					frame_rate
					);
			} else {
				LOG_GENERAL("Not using {} as it is: {}", sound_mxf->path(0).string(), why_not);
			}
			continue;
		}

		if (auto video_mxf = dynamic_pointer_cast<VideoMXFContent>(content)) {
			std::string why_not;
			if (video_mxf->can_reference_video(film, why_not)) {
				auto const frame_rate = film->video_frame_rate();
				auto const source = video_mxf->path(0);
				auto const in_dcp = film->dir(film->dcp_name()) / ("j2c_" + dcp::MonoJ2KPictureAsset(source).id() + ".mxf");
				put_in_dcp(source, in_dcp);

				maybe_add_asset(
					reel_assets,
					std::make_shared<dcp::ReelMonoPictureAsset>(std::make_shared<dcp::MonoJ2KPictureAsset>(in_dcp), 0),
					video_mxf->trim_start().frames_round(frame_rate),
					video_mxf->trim_end().frames_round(frame_rate),
					video_mxf->position(),
					frame_rate
					);
			} else {
				LOG_GENERAL("Not using {} as it is: {}", video_mxf->path(0).string(), why_not);
			}
			continue;
		}

		auto dcp = dynamic_pointer_cast<DCPContent>(content);
		if (!dcp) {
			continue;
		}

		if (!dcp->reference_video() && !dcp->reference_audio() && !dcp->reference_atmos() && !dcp->reference_text(TextType::OPEN_SUBTITLE) && !dcp->reference_text(TextType::CLOSED_CAPTION)) {
			continue;
		}

		scoped_ptr<DCPDecoder> decoder;
		try {
			decoder.reset(new DCPDecoder(film, dcp, false, false, shared_ptr<DCPDecoder>()));
		} catch (...) {
			return reel_assets;
		}

		auto const frame_rate = film->video_frame_rate();
		DCPOMATIC_ASSERT(dcp->video_frame_rate());
		/* We should only be referencing if the DCP rate is the same as the film rate */
		DCPOMATIC_ASSERT(std::round(dcp->video_frame_rate().get()) == frame_rate);

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
				maybe_add_asset(reel_assets, reel->main_picture(), reel_trim_start, reel_trim_end, from, frame_rate);
			}

			if (dcp->reference_audio()) {
				maybe_add_asset(reel_assets, reel->main_sound(), reel_trim_start, reel_trim_end, from, frame_rate);
			}

			if (dcp->reference_atmos() && reel->atmos()) {
				maybe_add_asset(reel_assets, reel->atmos(), reel_trim_start, reel_trim_end, from, frame_rate);
			}

			if (dcp->reference_text(TextType::OPEN_SUBTITLE) && reel->main_subtitle()) {
				maybe_add_asset(reel_assets, reel->main_subtitle(), reel_trim_start, reel_trim_end, from, frame_rate);
			}

			if (dcp->reference_text(TextType::CLOSED_CAPTION)) {
				for (auto caption: reel->closed_captions()) {
					maybe_add_asset(reel_assets, caption, reel_trim_start, reel_trim_end, from, frame_rate);
				}
			}

			offset_from_start += reel_duration;
			offset_from_end -= reel_duration;
		}
	}

	return reel_assets;
}

