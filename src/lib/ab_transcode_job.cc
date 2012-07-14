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

#include <stdexcept>
#include "ab_transcode_job.h"
#include "j2k_wav_encoder.h"
#include "film.h"
#include "format.h"
#include "filter.h"
#include "ab_transcoder.h"
#include "film_state.h"
#include "encoder_factory.h"
#include "config.h"

using namespace std;
using namespace boost;

/** @param s FilmState to compare (with filters and/or a non-bicubic scaler).
 *  @param o Options.
 *  @Param l A log that we can write to.
 */
ABTranscodeJob::ABTranscodeJob (shared_ptr<const FilmState> s, shared_ptr<const Options> o, Log* l)
	: Job (s, o, l)
{
	_fs_b.reset (new FilmState (*_fs));
	_fs_b->scaler = Config::instance()->reference_scaler ();
	_fs_b->filters = Config::instance()->reference_filters ();
}

string
ABTranscodeJob::name () const
{
	stringstream s;
	s << "A/B transcode " << _fs->name;
	return s.str ();
}

void
ABTranscodeJob::run ()
{
	try {
		/* _fs_b is the one with no filters */
		ABTranscoder w (_fs_b, _fs, _opt, this, _log, encoder_factory (_fs, _opt, _log));
		w.go ();
		set_progress (1);
		set_state (FINISHED_OK);

	} catch (std::exception& e) {

		set_state (FINISHED_ERROR);

	}
}
