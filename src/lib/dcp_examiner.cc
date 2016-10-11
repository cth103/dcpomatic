/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include "dcp_examiner.h"
#include "dcp_content.h"
#include "exceptions.h"
#include "image.h"
#include "config.h"
#include <dcp/dcp.h>
#include <dcp/decrypted_kdm.h>
#include <dcp/cpl.h>
#include <dcp/reel.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/mono_picture_asset.h>
#include <dcp/mono_picture_asset_reader.h>
#include <dcp/mono_picture_frame.h>
#include <dcp/stereo_picture_asset.h>
#include <dcp/stereo_picture_asset_reader.h>
#include <dcp/stereo_picture_frame.h>
#include <dcp/sound_asset.h>
#include <dcp/sound_asset_reader.h>
#include <dcp/subtitle_asset.h>
#include <dcp/reel_subtitle_asset.h>
#include <dcp/sound_asset.h>
#include <boost/foreach.hpp>
#include <iostream>

#include "i18n.h"

using std::list;
using std::cout;
using std::runtime_error;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

DCPExaminer::DCPExaminer (shared_ptr<const DCPContent> content)
	: DCP (content)
	, _video_length (0)
	, _audio_length (0)
	, _has_subtitles (false)
	, _encrypted (false)
	, _needs_assets (false)
	, _kdm_valid (false)
	, _three_d (false)
{
	shared_ptr<dcp::CPL> cpl;

	if (content->cpl ()) {
		/* Use the CPL that the content was using before */
		BOOST_FOREACH (shared_ptr<dcp::CPL> i, cpls()) {
			if (i->id() == content->cpl().get()) {
				cpl = i;
			}
		}
	} else {
		/* Choose the CPL with the fewest unsatisfied references */

		int least_unsatisfied = INT_MAX;

		BOOST_FOREACH (shared_ptr<dcp::CPL> i, cpls()) {
			int unsatisfied = 0;
			BOOST_FOREACH (shared_ptr<dcp::Reel> j, i->reels()) {
				if (j->main_picture() && !j->main_picture()->asset_ref().resolved()) {
					++unsatisfied;
				}
				if (j->main_sound() && !j->main_sound()->asset_ref().resolved()) {
					++unsatisfied;
				}
				if (j->main_subtitle() && !j->main_subtitle()->asset_ref().resolved()) {
					++unsatisfied;
				}
			}

			if (unsatisfied < least_unsatisfied) {
				least_unsatisfied = unsatisfied;
				cpl = i;
			}
		}
	}

	if (!cpl) {
		throw DCPError ("No CPLs found in DCP");
	}

	_cpl = cpl->id ();
	_name = cpl->content_title_text ();

	BOOST_FOREACH (shared_ptr<dcp::Reel> i, cpl->reels()) {

		if (i->main_picture ()) {
			if (!i->main_picture()->asset_ref().resolved()) {
				/* We are missing this asset so we can't continue; examination will be repeated later */
				_needs_assets = true;
				return;
			}

			dcp::Fraction const frac = i->main_picture()->edit_rate ();
			float const fr = float(frac.numerator) / frac.denominator;
			if (!_video_frame_rate) {
				_video_frame_rate = fr;
			} else if (_video_frame_rate.get() != fr) {
				throw DCPError (_("Mismatched frame rates in DCP"));
			}

			shared_ptr<dcp::PictureAsset> asset = i->main_picture()->asset ();
			if (!_video_size) {
				_video_size = asset->size ();
			} else if (_video_size.get() != asset->size ()) {
				throw DCPError (_("Mismatched video sizes in DCP"));
			}

			_video_length += i->main_picture()->duration();
		}

		if (i->main_sound ()) {
			if (!i->main_sound()->asset_ref().resolved()) {
				/* We are missing this asset so we can't continue; examination will be repeated later */
				_needs_assets = true;
				return;
			}

			shared_ptr<dcp::SoundAsset> asset = i->main_sound()->asset ();

			if (!_audio_channels) {
				_audio_channels = asset->channels ();
			} else if (_audio_channels.get() != asset->channels ()) {
				throw DCPError (_("Mismatched audio channel counts in DCP"));
			}

			if (!_audio_frame_rate) {
				_audio_frame_rate = asset->sampling_rate ();
			} else if (_audio_frame_rate.get() != asset->sampling_rate ()) {
				throw DCPError (_("Mismatched audio sample rates in DCP"));
			}

			_audio_length += i->main_sound()->duration();
		}

		if (i->main_subtitle ()) {
			if (!i->main_subtitle()->asset_ref().resolved()) {
				/* We are missing this asset so we can't continue; examination will be repeated later */
				_needs_assets = true;
				return;
			}

			_has_subtitles = true;
		}
	}

	_encrypted = cpl->encrypted ();
	_kdm_valid = true;

	/* Check that we can read the first picture, sound and subtitle frames of each reel */
	try {
		BOOST_FOREACH (shared_ptr<dcp::Reel> i, cpl->reels()) {
			shared_ptr<dcp::PictureAsset> pic = i->main_picture()->asset ();
			shared_ptr<dcp::MonoPictureAsset> mono = dynamic_pointer_cast<dcp::MonoPictureAsset> (pic);
			shared_ptr<dcp::StereoPictureAsset> stereo = dynamic_pointer_cast<dcp::StereoPictureAsset> (pic);

			if (mono) {
				mono->start_read()->get_frame(0)->xyz_image ();
			} else {
				stereo->start_read()->get_frame(0)->xyz_image (dcp::EYE_LEFT);
			}

			if (i->main_sound()) {
				shared_ptr<dcp::SoundAsset> sound = i->main_sound()->asset ();
				i->main_sound()->asset()->start_read()->get_frame(0);
			}

			if (i->main_subtitle()) {
				i->main_subtitle()->asset()->subtitles ();
			}
		}
	} catch (dcp::DCPReadError& e) {
		_kdm_valid = false;
	}

	DCPOMATIC_ASSERT (cpl->standard ());
	_standard = cpl->standard().get();
	_three_d = !cpl->reels().empty() && cpl->reels().front()->main_picture() &&
		dynamic_pointer_cast<dcp::StereoPictureAsset> (cpl->reels().front()->main_picture()->asset());

	_cpl = cpl->id ();
}
