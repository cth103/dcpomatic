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
#include <iostream>

#include "i18n.h"

using std::list;
using std::cout;
using std::runtime_error;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

DCPExaminer::DCPExaminer (shared_ptr<const DCPContent> content)
	: _video_length (0)
	, _audio_length (0)
	, _has_subtitles (false)
	, _encrypted (false)
	, _kdm_valid (false)
	, _three_d (false)
{
	dcp::DCP dcp (content->directory ());
	dcp.read (false, 0, true);

	if (content->kdm ()) {
		dcp.add (dcp::DecryptedKDM (content->kdm().get(), Config::instance()->decryption_chain()->key().get ()));
	}

	if (dcp.cpls().size() == 0) {
		throw DCPError ("No CPLs found in DCP");
	} else if (dcp.cpls().size() > 1) {
		throw DCPError ("Multiple CPLs found in DCP");
	}

	_name = dcp.cpls().front()->content_title_text ();

	list<shared_ptr<dcp::Reel> > reels = dcp.cpls().front()->reels ();
	for (list<shared_ptr<dcp::Reel> >::const_iterator i = reels.begin(); i != reels.end(); ++i) {

		if ((*i)->main_picture ()) {
			dcp::Fraction const frac = (*i)->main_picture()->edit_rate ();
			float const fr = float(frac.numerator) / frac.denominator;
			if (!_video_frame_rate) {
				_video_frame_rate = fr;
			} else if (_video_frame_rate.get() != fr) {
				throw DCPError (_("Mismatched frame rates in DCP"));
			}

			shared_ptr<dcp::PictureAsset> asset = (*i)->main_picture()->asset ();
			if (!_video_size) {
				_video_size = asset->size ();
			} else if (_video_size.get() != asset->size ()) {
				throw DCPError (_("Mismatched video sizes in DCP"));
			}

			_video_length += (*i)->main_picture()->duration();
		}

		if ((*i)->main_sound ()) {
			shared_ptr<dcp::SoundAsset> asset = (*i)->main_sound()->asset ();

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

			_audio_length += (*i)->main_sound()->duration();
		}

		if ((*i)->main_subtitle ()) {
			_has_subtitles = true;
		}
	}

	_encrypted = dcp.encrypted ();
	_kdm_valid = true;

	/* Check that we can read the first picture frame */
	try {
		if (!dcp.cpls().empty () && !dcp.cpls().front()->reels().empty ()) {
			shared_ptr<dcp::PictureAsset> asset = dcp.cpls().front()->reels().front()->main_picture()->asset ();
			shared_ptr<dcp::MonoPictureAsset> mono = dynamic_pointer_cast<dcp::MonoPictureAsset> (asset);
			shared_ptr<dcp::StereoPictureAsset> stereo = dynamic_pointer_cast<dcp::StereoPictureAsset> (asset);

			if (mono) {
				mono->start_read()->get_frame(0)->xyz_image ();
			} else {
				stereo->start_read()->get_frame(0)->xyz_image (dcp::EYE_LEFT);
			}

		}
	} catch (dcp::DCPReadError& e) {
		_kdm_valid = false;
		if (_encrypted && content->kdm ()) {
			/* XXX: maybe don't use an exception for this */
			throw runtime_error (_("The KDM does not decrypt the DCP.  Perhaps it is targeted at the wrong CPL."));
		}
	}

	_standard = dcp.standard ();
	_three_d = !reels.empty() && reels.front()->main_picture() &&
		dynamic_pointer_cast<dcp::StereoPictureAsset> (reels.front()->main_picture()->asset());
}
