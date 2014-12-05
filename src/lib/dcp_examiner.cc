/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "dcp_examiner.h"
#include "dcp_content.h"
#include "exceptions.h"
#include "image.h"
#include "config.h"
#include <dcp/dcp.h>
#include <dcp/cpl.h>
#include <dcp/reel.h>
#include <dcp/reel_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/mono_picture_mxf.h>
#include <dcp/mono_picture_frame.h>
#include <dcp/stereo_picture_mxf.h>
#include <dcp/stereo_picture_frame.h>
#include <dcp/sound_mxf.h>

#include "i18n.h"

using std::list;
using std::cout;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

DCPExaminer::DCPExaminer (shared_ptr<const DCPContent> content)
	: _video_length (0)
	, _audio_length (0)
	, _has_subtitles (false)
	, _encrypted (false)
	, _kdm_valid (false)
{
	dcp::DCP dcp (content->directory ());
	dcp.read ();

	if (content->kdm ()) {
		dcp.add (dcp::DecryptedKDM (content->kdm().get(), Config::instance()->decryption_private_key ()));
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
			dcp::Fraction const frac = (*i)->main_picture()->frame_rate ();
			float const fr = float(frac.numerator) / frac.denominator;
			if (!_video_frame_rate) {
				_video_frame_rate = fr;
			} else if (_video_frame_rate.get() != fr) {
				throw DCPError (_("Mismatched frame rates in DCP"));
			}

			shared_ptr<dcp::PictureMXF> mxf = (*i)->main_picture()->mxf ();
			if (!_video_size) {
				_video_size = mxf->size ();
			} else if (_video_size.get() != mxf->size ()) {
				throw DCPError (_("Mismatched video sizes in DCP"));
			}

			_video_length += ContentTime::from_frames ((*i)->main_picture()->duration(), _video_frame_rate.get ());
		}
			
		if ((*i)->main_sound ()) {
			shared_ptr<dcp::SoundMXF> mxf = (*i)->main_sound()->mxf ();

			if (!_audio_channels) {
				_audio_channels = mxf->channels ();
			} else if (_audio_channels.get() != mxf->channels ()) {
				throw DCPError (_("Mismatched audio channel counts in DCP"));
			}

			if (!_audio_frame_rate) {
				_audio_frame_rate = mxf->sampling_rate ();
			} else if (_audio_frame_rate.get() != mxf->sampling_rate ()) {
				throw DCPError (_("Mismatched audio frame rates in DCP"));
			}

			_audio_length += ContentTime::from_frames ((*i)->main_sound()->duration(), _video_frame_rate.get ());
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
			shared_ptr<dcp::PictureMXF> mxf = dcp.cpls().front()->reels().front()->main_picture()->mxf ();
			shared_ptr<dcp::MonoPictureMXF> mono = dynamic_pointer_cast<dcp::MonoPictureMXF> (mxf);
			shared_ptr<dcp::StereoPictureMXF> stereo = dynamic_pointer_cast<dcp::StereoPictureMXF> (mxf);
			
			shared_ptr<Image> image (new Image (PIX_FMT_RGB24, _video_size.get(), false));
			
			if (mono) {
				mono->get_frame(0)->rgb_frame (image->data()[0]);
			} else {
				stereo->get_frame(0)->rgb_frame (dcp::EYE_LEFT, image->data()[0]);
			}
			
		}
	} catch (dcp::DCPReadError& e) {
		_kdm_valid = false;
		if (_encrypted && content->kdm ()) {
			/* XXX: maybe don't use an exception for this */
			throw StringError (_("The KDM does not decrypt the DCP.  Perhaps it is targeted at the wrong CPL"));
		}
	}
}
