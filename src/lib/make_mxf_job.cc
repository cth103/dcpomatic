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

/** @file  src/make_mxf_job.cc
 *  @brief A job that creates a MXF file from some data.
 */

#include <iostream>
#include <boost/filesystem.hpp>
#include "AS_DCP.h"
#include "make_mxf_job.h"
#include "film.h"
#include "film_state.h"
#include "options.h"
#include "exceptions.h"

using namespace std;
using namespace boost;

/** @class MakeMXFJob
 *  @brief A job that creates a MXF file from some data.
 */

MakeMXFJob::MakeMXFJob (shared_ptr<const FilmState> s, shared_ptr<const Options> o, Log* l, Type t)
	: Job (s, o, l)
	, _type (t)
{

}

string
MakeMXFJob::name () const
{
	stringstream s;
	switch (_type) {
	case VIDEO:
		s << "Make video MXF for " << _fs->name;
		break;
	case AUDIO:
		s << "Make audio MXF for " << _fs->name;
		break;
	}
	
	return s.str ();
}

#if 0
XXX
void
MakeMXFJob::run ()
{
	set_progress_unknown ();

	/* We round for DCP: not sure if this is right */
	float fps = rintf (_fs->frames_per_second);
	
	stringstream c;
	c << "opendcp_mxf -r " << fps << " -i ";
	switch (_type) {
	case VIDEO:
		c << "\"" << _opt->frame_out_path () << "\" -o \"" << _fs->file ("video.mxf") << "\"";
		break;
	case AUDIO:
		c << "\"" << _opt->multichannel_audio_out_path () << "/*\" -o \"" << _fs->file ("audio.mxf") << "\"";
		break;
	}

	command (c.str ());
	set_progress (1);
}
#endif

void
MakeMXFJob::run ()
{
	set_progress (0);

	string dir;
	switch (_type) {
	case VIDEO:
		dir = _opt->frame_out_path ();
		break;
	case AUDIO:
		dir = _opt->multichannel_audio_out_path ();
		break;
	}

	std::list<std::string> files;
        for (filesystem::directory_iterator i = filesystem::directory_iterator (dir); i != filesystem::directory_iterator(); ++i) {
		files.push_back (filesystem::path (*i).string());
	}

	if (files.empty ()) {
		throw EncodeError ("no input files found for MXF");
	}

	ASDCP::EssenceType_t essence_type;
	if (ASDCP_FAILURE (ASDCP::RawEssenceType (files.front().c_str(), essence_type))) {
		throw EncodeError ("could not work out type for MXF");
	}

	switch (essence_type) {
	case ASDCP::ESS_JPEG_2000:
		/* XXX */
		break;
	case ASDCP::ESS_PCM_24b_48k:
	case ASDCP::ESS_PCM_24b_96k:
		/* XXX */
		break;
	default:
		throw EncodeError ("unknown essence type");
	}
	
	set_progress (1);
}
