/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

/** @file src/make_dcp_job.cc
 *  @brief A job to create DCPs.
 */

#include <boost/filesystem.hpp>
#include <libdcp/dcp.h>
#include <libdcp/picture_asset.h>
#include <libdcp/sound_asset.h>
#include <libdcp/reel.h>
extern "C" {
#include <libavutil/pixdesc.h>
}
#include "make_dcp_job.h"
#include "dcp_content_type.h"
#include "exceptions.h"
#include "options.h"
#include "imagemagick_decoder.h"
#include "film.h"

using std::string;
using boost::shared_ptr;

/** @param f Film we are making the DCP for.
 *  @param o Options.
 */
MakeDCPJob::MakeDCPJob (shared_ptr<Film> f, shared_ptr<const Options> o, shared_ptr<Job> req)
	: Job (f, req)
	, _opt (o)
{
	
}

string
MakeDCPJob::name () const
{
	return String::compose ("Make DCP for %1", _film->name());
}

string
MakeDCPJob::j2c_path (int f) const
{
	return _opt->frame_out_path (f, false);
}

string
MakeDCPJob::wav_path (libdcp::Channel c) const
{
	return _opt->multichannel_audio_out_path (int (c), false);
}

void
MakeDCPJob::run ()
{
	string const dcp_path = _film->dir (_film->dcp_name());

	/* Remove any old DCP */
	boost::filesystem::remove_all (dcp_path);

	int frames = 0;
	switch (_film->content_type ()) {
	case VIDEO:
		frames = _film->dcp_length ();
		break;
	case STILL:
		frames = _film->still_duration() * ImageMagickDecoder::static_frames_per_second ();
		break;
	}
	
	libdcp::DCP dcp (_film->dir (_film->dcp_name()));
	dcp.Progress.connect (sigc::mem_fun (*this, &MakeDCPJob::dcp_progress));

	shared_ptr<libdcp::CPL> cpl (
		new libdcp::CPL (_film->dir (_film->dcp_name()), _film->dcp_name(), _film->dcp_content_type()->libdcp_kind (), frames, rint (_film->frames_per_second()))
		);
	
	dcp.add_cpl (cpl);

	descend (0.9);
	shared_ptr<libdcp::MonoPictureAsset> pa (
		new libdcp::MonoPictureAsset (
			sigc::mem_fun (*this, &MakeDCPJob::j2c_path),
			_film->dir (_film->dcp_name()),
			"video.mxf",
			&dcp.Progress,
			rint (_film->frames_per_second()),
			frames,
			_opt->out_size.width,
			_opt->out_size.height
			)
		);
	
	ascend ();

	shared_ptr<libdcp::SoundAsset> sa;

	if (_film->audio_channels() > 0) {
		descend (0.1);
		sa.reset (
			new libdcp::SoundAsset (
				sigc::mem_fun (*this, &MakeDCPJob::wav_path),
				_film->dir (_film->dcp_name()),
				"audio.mxf",
				&dcp.Progress,
				rint (_film->frames_per_second()),
				frames,
				_film->audio_channels()
				)
			);
		ascend ();
	}

	cpl->add_reel (shared_ptr<libdcp::Reel> (new libdcp::Reel (pa, sa, shared_ptr<libdcp::SubtitleAsset> ())));
	dcp.write_xml ();

	set_progress (1);
	set_state (FINISHED_OK);
}

void
MakeDCPJob::dcp_progress (float p)
{
	set_progress (p);
}
