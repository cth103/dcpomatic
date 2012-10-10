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

/** @file src/thumbs_job.cc
 *  @brief A job to create thumbnails.
 */

#include <exception>
#include "thumbs_job.h"
#include "film_state.h"
#include "tiff_encoder.h"
#include "transcoder.h"
#include "options.h"

using namespace std;
using namespace boost;

/** @param s FilmState to use.
 *  @param o Options.
 *  @param l A log that we can write to.
 */
ThumbsJob::ThumbsJob (shared_ptr<const FilmState> s, shared_ptr<const Options> o, Log* l, shared_ptr<Job> req)
	: Job (s, o, l, req)
{
	
}

string
ThumbsJob::name () const
{
	return String::compose ("Update thumbs for %1", _fs->name);
}

void
ThumbsJob::run ()
{
	try {
		shared_ptr<TIFFEncoder> e (new TIFFEncoder (_fs, _opt, _log));
		Transcoder w (_fs, _opt, this, _log, e);
		w.go ();
		set_progress (1);
		set_state (FINISHED_OK);

	} catch (std::exception& e) {

		set_progress (1);
		set_error (e.what ());
		set_state (FINISHED_ERROR);
	}
}
