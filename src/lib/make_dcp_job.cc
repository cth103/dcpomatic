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
extern "C" {
#include <libavutil/pixdesc.h>
}
#include "make_dcp_job.h"
#include "film_state.h"
#include "dcp_content_type.h"
#include "exceptions.h"

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
	set_progress_unknown ();

	string const dcp_path = _fs->dir (_fs->name);
	
	/* Check that we have our prerequisites */

	if (!filesystem::exists (filesystem::path (_fs->file ("video.mxf")))) {
		throw EncodeError ("missing video.mxf");
	}

	bool const have_audio = filesystem::exists (filesystem::path (_fs->file ("audio.mxf")));

	/* Remove any old DCP */
	filesystem::remove_all (dcp_path);

	DCP dcp (_fs->dir (_fs->name()));
	dcp.add_asset (
		shared_ptr<MainPictureAsset> (new MainPictureAsset ("video.mxf", rint (_fs->frames_per_second), _fs->length, _opt->out_size))
		);

	if (filesystem::exists (filesystem::path (_fs->file ("audio.mxf")))) {
		dcp.add_asset (
			shared_ptr<MainSoundAsset> (new MainSoundAsset ("audio.mxf", rint (_fs->frames_per_second), _fs->length))
			);
	}
								    
	dcp.write_xml ();


	add_pkl ();
	add_cpl (pkl[0]);

	add_reel (pkl[0].cpl[0]);

	write_cpl ();
	write_pkl ();
	write_volindex ();
	write_assetmap ();


	set_progress (1);
}
