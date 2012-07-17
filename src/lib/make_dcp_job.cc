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

void
MakeDCPJob::run ()
{
	string const dcp_path = _fs->dir (_fs->name);

	/* Remove any old DCP */
	filesystem::remove_all (dcp_path);

	libdcp::DCP dcp (_fs->dir (_fs->name), _fs->name, _fs->dcp_content_type->libdcp_type (), rint (_fs->frames_per_second), _fs->length);
	dcp.Progress.connect (sigc::mem_fun (*this, &MakeDCPJob::dcp_progress));

	list<string> j2cs;
	int f = _fs->dcp_frames ? _fs->dcp_frames : _fs->length;
	for (int i = 0; i < f; ++i) {
		j2cs.push_back (_opt->frame_out_path (i, false));
	}

	descend (0.9);
	dcp.add_picture_asset (j2cs, _opt->out_size.width, _opt->out_size.height);
	ascend ();

	list<string> wavs;
	for (int i = 0; i < _fs->audio_channels; ++i) {
		wavs.push_back (_opt->multichannel_audio_out_path (i, false));
	}

	if (!wavs.empty ()) {
		descend (0.1);
		dcp.add_sound_asset (wavs);
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
