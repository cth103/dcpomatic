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

/** @file src/ab_transcode_job.h
 *  @brief Job to run a transcoder which produces output for A/B comparison of various settings.
 */

#include <boost/shared_ptr.hpp>
#include "job.h"

/** @class ABTranscodeJob
 *  @brief Job to run a transcoder which produces output for A/B comparison of various settings.
 *
 *  The right half of the frame will be processed using the FilmState supplied;
 *  the left half will be processed using the same state but *without* filters
 *  and with the scaler set to SWS_BICUBIC.
 */
class ABTranscodeJob : public Job
{
public:
	ABTranscodeJob (boost::shared_ptr<const FilmState> s, boost::shared_ptr<const Options> o, Log* l, boost::shared_ptr<Job> req);

	std::string name () const;
	void run ();

private:
	boost::shared_ptr<const Options> _opt;
	
	/** Copy of our FilmState with filters removed and scaler set back to bicubic;
	 *  this is the `reference' (left-half-frame) state.
	 */
	boost::shared_ptr<FilmState> _fs_b;
};
