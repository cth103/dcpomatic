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
extern "C" {
#include <libavutil/pixdesc.h>
}
#include "make_dcp_job.h"
#include "film_state.h"
#include "dcp_content_type.h"
#include "exceptions.h"
#include "options.h"
#include "imagemagick_decoder.h"

using namespace std;
using namespace boost;

/** @param s FilmState of the Film we are making the DCP for.
 *  @param o Options.
 *  @param l Log.
 */
MakeDCPJob::MakeDCPJob (shared_ptr<const FilmState> s, shared_ptr<const Options> o, Log* l)
	: Job (s, o, l)
{
	
}

string
MakeDCPJob::name () const
{
	stringstream s;
	s << "Make DCP for " << _fs->name;
	return s.str ();
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
	string const dcp_path = _fs->dir (_fs->name);

	/* Remove any old DCP */
	filesystem::remove_all (dcp_path);

	int frames = 0;
	switch (_fs->content_type ()) {
	case VIDEO:
		frames = _fs->dcp_frames ? _fs->dcp_frames : _fs->length;
		break;
	case STILL:
		frames = _fs->still_duration * ImageMagickDecoder::static_frames_per_second ();
		break;
	}
	
	libdcp::DCP dcp (_fs->dir (_fs->name), _fs->name, _fs->dcp_content_type->libdcp_kind (), rint (_fs->frames_per_second), frames);
	dcp.Progress.connect (sigc::mem_fun (*this, &MakeDCPJob::dcp_progress));

	descend (0.9);
	dcp.add_picture_asset (sigc::mem_fun (*this, &MakeDCPJob::j2c_path), _opt->out_size.width, _opt->out_size.height);
	ascend ();

	if (_fs->audio_channels > 0) {
		descend (0.1);
		dcp.add_sound_asset (sigc::mem_fun (*this, &MakeDCPJob::wav_path), _fs->audio_channels);
		ascend ();
	}

	dcp.write_xml ();

	set_progress (1);
	set_state (FINISHED_OK);
}

void
MakeDCPJob::dcp_progress (float p)
{
	set_progress (p);
}
