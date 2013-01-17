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

#include <iostream>
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
#include "format.h"

using std::string;
using std::cout;
using boost::shared_ptr;

/** @param f Film we are making the DCP for.
 *  @param o Options.
 */
MakeDCPJob::MakeDCPJob (shared_ptr<Film> f, shared_ptr<Job> req)
	: Job (f, req)
{
	
}

string
MakeDCPJob::name () const
{
	return String::compose ("Make DCP for %1", _film->name());
}

/** @param f DCP frame index */
string
MakeDCPJob::j2c_path (int f, int offset) const
{
	return _film->frame_out_path (f + offset, false);
}

string
MakeDCPJob::wav_path (libdcp::Channel c) const
{
	return _film->multichannel_audio_out_path (int (c), false);
}

void
MakeDCPJob::run ()
{
	if (!_film->dcp_intrinsic_duration()) {
		throw EncodeError ("cannot make a DCP when its intrinsic duration is not known");
	}

	descend (0.9);
	
	string const dcp_path = _film->dir (_film->dcp_name());

	/* Remove any old DCP */
	boost::filesystem::remove_all (dcp_path);

	int const frames = _film->dcp_intrinsic_duration().get();
	int const duration = frames - _film->trim_start() - _film->trim_end();
	DCPFrameRate const dfr (_film->frames_per_second ());

	libdcp::DCP dcp (_film->dir (_film->dcp_name()));
	dcp.Progress.connect (boost::bind (&MakeDCPJob::dcp_progress, this, _1));

	shared_ptr<libdcp::CPL> cpl (
		new libdcp::CPL (_film->dir (_film->dcp_name()), _film->dcp_name(), _film->dcp_content_type()->libdcp_kind (), frames, dfr.frames_per_second)
		);
	
	dcp.add_cpl (cpl);

	int frames_per_reel = 0;
	if (_film->reel_size()) {
		frames_per_reel = (_film->reel_size().get() / (_film->j2k_bandwidth() / 8)) * dfr.frames_per_second;
	} else {
		frames_per_reel = frames;
	}

	int frames_done = 0;
	int reel = 0;

	while (frames_done < frames) {

		descend (float (frames_per_reel) / frames);

		int this_time = std::min (frames_per_reel, (frames - frames_done));

		descend (0.8);

		shared_ptr<libdcp::MonoPictureAsset> pa (
			new libdcp::MonoPictureAsset (
				boost::bind (&MakeDCPJob::j2c_path, this, _1, frames_done),
				_film->dir (_film->dcp_name()),
				String::compose ("video_%1.mxf", reel),
				&dcp.Progress,
				dfr.frames_per_second,
				this_time,
				_film->format()->dcp_size()
				)
			);

		pa->set_entry_point (_film->trim_start ());
		pa->set_duration (duration);
	
		ascend ();
		
		shared_ptr<libdcp::SoundAsset> sa;
		
		if (_film->audio_channels() > 0) {
			descend (0.1);
			sa.reset (
				new libdcp::SoundAsset (
					boost::bind (&MakeDCPJob::wav_path, this, _1),
					_film->dir (_film->dcp_name()),
					String::compose ("audio_%1.mxf", reel),
					&dcp.Progress,
					dfr.frames_per_second,
					this_time,
					frames_done,
					dcp_audio_channels (_film->audio_channels())
					)
				);

			sa->set_entry_point (_film->trim_start ());
			sa->set_duration (duration);
			
			ascend ();
		}

		descend (0.1);
		cpl->add_reel (shared_ptr<libdcp::Reel> (new libdcp::Reel (pa, sa, shared_ptr<libdcp::SubtitleAsset> ())));
		ascend ();
		
		frames_done += frames_per_reel;
		++reel;

		ascend ();
	}

	ascend ();

	descend (0.1);
	dcp.write_xml ();
	ascend ();
		
	set_progress (1);
	set_state (FINISHED_OK);
}

void
MakeDCPJob::dcp_progress (float p)
{
	set_progress (p);
}
