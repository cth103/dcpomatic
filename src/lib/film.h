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

/** @file  src/film.h
 *  @brief A representation of a piece of video (with sound), including naming,
 *  the source content file, and how it should be presented in a DCP.
 */

#ifndef DVDOMATIC_FILM_H
#define DVDOMATIC_FILM_H

#include <string>
#include <vector>
#include <inttypes.h>
#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>
extern "C" {
#include <libavcodec/avcodec.h>
}
#include "dcp_content_type.h"
#include "film_state.h"

class Format;
class Job;
class Filter;
class Log;
class ExamineContentJob;

/** @class Film
 *  @brief A representation of a video with sound.
 *
 *  A representation of a piece of video (with sound), including naming,
 *  the source content file, and how it should be presented in a DCP.
 */
class Film : public FilmState
{
public:
	Film (std::string d, bool must_exist = true);
	~Film ();

	std::string j2k_dir () const;

	std::vector<std::string> audio_files () const;

	void update_thumbs_pre_gui ();
	void update_thumbs_post_gui ();
	std::pair<Position, std::string> thumb_subtitle (int) const;

	void copy_from_dvd_post_gui ();
	void examine_content ();
	void examine_content_post_gui ();
	void send_dcp_to_tms ();
	void copy_from_dvd ();

	void make_dcp (bool);

	/** @return Logger.
	 *  It is safe to call this from any thread.
	 */
	Log* log () const {
		return _log;
	}

	int encoded_frames () const;
	
private:
	/** Log to write to */
	Log* _log;

	/** Any running ExamineContentJob, or 0 */
	boost::shared_ptr<ExamineContentJob> _examine_content_job;
};

#endif
