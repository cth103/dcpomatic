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
#include "film.h"
#include "format.h"
#include "filter.h"
#include "ab_transcoder.h"
#include "config.h"
#include "encoder.h"

#include "i18n.h"

using std::string;
using boost::shared_ptr;

/** @param f Film to compare.
 *  @param o Decode options.
 */
ABTranscodeJob::ABTranscodeJob (shared_ptr<Film> f, DecodeOptions o)
	: Job (f)
	, _decode_opt (o)
{
	_film_b.reset (new Film (*_film));
	_film_b->set_scaler (Config::instance()->reference_scaler ());
	_film_b->set_filters (Config::instance()->reference_filters ());
}

string
ABTranscodeJob::name () const
{
	return String::compose (_("A/B transcode %1"), _film->name());
}

void
ABTranscodeJob::run ()
{
	try {
		/* _film_b is the one with reference filters */
		ABTranscoder w (_film_b, _film, _decode_opt, this, shared_ptr<Encoder> (new Encoder (_film)));
		w.go ();
		set_progress (1);
		set_state (FINISHED_OK);

	} catch (std::exception& e) {

		set_state (FINISHED_ERROR);

	}
}
