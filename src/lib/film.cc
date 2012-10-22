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
#include <iostream>
#include <algorithm>
#include <fstream>
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <unistd.h>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include "film.h"
#include "format.h"
#include "imagemagick_encoder.h"
#include "job.h"
#include "filter.h"
#include "transcoder.h"
#include "util.h"
#include "job_manager.h"
#include "ab_transcode_job.h"
#include "transcode_job.h"
#include "scp_dcp_job.h"
#include "copy_from_dvd_job.h"
#include "make_dcp_job.h"
#include "film_state.h"
#include "log.h"
#include "options.h"
#include "exceptions.h"
#include "examine_content_job.h"
#include "scaler.h"
#include "decoder_factory.h"
#include "config.h"
#include "check_hashes_job.h"
#include "version.h"

using namespace std;
using namespace boost;

/** Construct a Film object in a given directory, reading any metadata
 *  file that exists in that directory.  An exception will be thrown if
 *  must_exist is true, and the specified directory does not exist.
 *
 *  @param d Film directory.
 *  @param must_exist true to throw an exception if does not exist.
 */

Film::Film (string d, bool must_exist)
{
	/* Make state.directory a complete path without ..s (where possible)
	   (Code swiped from Adam Bowen on stackoverflow)
	*/
	
	filesystem::path p (filesystem::system_complete (d));
	filesystem::path result;
	for (filesystem::path::iterator i = p.begin(); i != p.end(); ++i) {
		if (*i == "..") {
			if (filesystem::is_symlink (result) || result.filename() == "..") {
				result /= *i;
			} else {
				result = result.parent_path ();
			}
		} else if (*i != ".") {
			result /= *i;
		}
	}

	set_directory (result.string ());
	
	if (!filesystem::exists (directory())) {
		if (must_exist) {
			throw OpenFileError (directory());
		} else {
			filesystem::create_directory (directory());
		}
	}

	read_metadata ();

	_log = new FileLog (file ("log"));
}

Film::~Film ()
{
	delete _log;
}
	  
/** @return The path to the directory to write JPEG2000 files to */
string
Film::j2k_dir () const
{
	assert (format());

	filesystem::path p;

	/* Start with j2c */
	p /= "j2c";

	pair<string, string> f = Filter::ffmpeg_strings (filters());

	/* Write stuff to specify the filter / post-processing settings that are in use,
	   so that we don't get confused about J2K files generated using different
	   settings.
	*/
	stringstream s;
	s << format()->id()
	  << "_" << content_digest()
	  << "_" << crop().left << "_" << crop().right << "_" << crop().top << "_" << crop().bottom
	  << "_" << f.first << "_" << f.second
	  << "_" << scaler()->id();

	p /= s.str ();

	/* Similarly for the A/B case */
	if (dcp_ab()) {
		stringstream s;
		pair<string, string> fa = Filter::ffmpeg_strings (Config::instance()->reference_filters());
		s << "ab_" << Config::instance()->reference_scaler()->id() << "_" << fa.first << "_" << fa.second;
		p /= s.str ();
	}
	
	return dir (p.string());
}

/** Add suitable Jobs to the JobManager to create a DCP for this Film.
 *  @param true to transcode, false to use the WAV and J2K files that are already there.
 */
void
Film::make_dcp (bool transcode)
{
	if (dcp_name().find ("/") != string::npos) {
		throw BadSettingError ("name", "cannot contain slashes");
	}
	
	log()->log (String::compose ("DVD-o-matic %1 git %2 using %3", dvdomatic_version, dvdomatic_git_commit, dependency_version_summary()));

	{
		char buffer[128];
		gethostname (buffer, sizeof (buffer));
		log()->log (String::compose ("Starting to make DCP on %1", buffer));
	}
		
	if (format() == 0) {
		throw MissingSettingError ("format");
	}

	if (content().empty ()) {
		throw MissingSettingError ("content");
	}

	if (dcp_content_type() == 0) {
		throw MissingSettingError ("content type");
	}

	if (name().empty()) {
		throw MissingSettingError ("name");
	}

	shared_ptr<const FilmState> fs = state_copy ();
	shared_ptr<Options> o (new Options (j2k_dir(), ".j2c", dir ("wavs")));
	o->out_size = format()->dcp_size ();
	if (dcp_frames() == 0) {
		/* Decode the whole film, no blacking */
		o->black_after = 0;
	} else {
		switch (dcp_trim_action()) {
		case CUT:
			/* Decode only part of the film, no blacking */
			o->black_after = 0;
			break;
		case BLACK_OUT:
			/* Decode the whole film, but black some frames out */
			o->black_after = dcp_frames ();
		}
	}
	
	o->padding = format()->dcp_padding (this);
	o->ratio = format()->ratio_as_float (this);
	o->decode_subtitles = with_subtitles ();

	shared_ptr<Job> r;

	if (transcode) {
		if (dcp_ab()) {
			r = JobManager::instance()->add (shared_ptr<Job> (new ABTranscodeJob (fs, o, log(), shared_ptr<Job> ())));
		} else {
			r = JobManager::instance()->add (shared_ptr<Job> (new TranscodeJob (fs, o, log(), shared_ptr<Job> ())));
		}
	}

	r = JobManager::instance()->add (shared_ptr<Job> (new CheckHashesJob (fs, o, log(), r)));
	JobManager::instance()->add (shared_ptr<Job> (new MakeDCPJob (fs, o, log(), r)));
}

void
Film::copy_from_dvd_post_gui ()
{
	const string dvd_dir = dir ("dvd");

	string largest_file;
	uintmax_t largest_size = 0;
	for (filesystem::directory_iterator i = filesystem::directory_iterator (dvd_dir); i != filesystem::directory_iterator(); ++i) {
		uintmax_t const s = filesystem::file_size (*i);
		if (s > largest_size) {

#if BOOST_FILESYSTEM_VERSION == 3		
			largest_file = filesystem::path(*i).generic_string();
#else
			largest_file = i->string ();
#endif
			largest_size = s;
		}
	}

	set_content (largest_file);
}

/** Start a job to examine our content file */
void
Film::examine_content ()
{
	if (_examine_content_job) {
		return;
	}

	set_thumbs (vector<int> ());
	filesystem::remove_all (dir ("thumbs"));

	/* This call will recreate the directory */
	dir ("thumbs");
	
	_examine_content_job.reset (new ExamineContentJob (state_copy (), log(), shared_ptr<Job> ()));
	_examine_content_job->Finished.connect (sigc::mem_fun (*this, &Film::examine_content_post_gui));
	JobManager::instance()->add (_examine_content_job);
}

void
Film::examine_content_post_gui ()
{
	set_length (_examine_content_job->last_video_frame ());
	_examine_content_job.reset ();

	string const tdir = dir ("thumbs");
	vector<int> thumbs;

	for (filesystem::directory_iterator i = filesystem::directory_iterator (tdir); i != filesystem::directory_iterator(); ++i) {

		/* Aah, the sweet smell of progress */
#if BOOST_FILESYSTEM_VERSION == 3		
		string const l = filesystem::path(*i).leaf().generic_string();
#else
		string const l = i->leaf ();
#endif
		
		size_t const d = l.find (".png");
		size_t const t = l.find (".tmp");
		if (d != string::npos && t == string::npos) {
			thumbs.push_back (atoi (l.substr (0, d).c_str()));
		}
	}

	sort (thumbs.begin(), thumbs.end());
	set_thumbs (thumbs);	
}


/** @return full paths to any audio files that this Film has */
vector<string>
Film::audio_files () const
{
	vector<string> f;
	for (filesystem::directory_iterator i = filesystem::directory_iterator (dir("wavs")); i != filesystem::directory_iterator(); ++i) {
		f.push_back (i->path().string ());
	}

	return f;
}

/** Start a job to send our DCP to the configured TMS */
void
Film::send_dcp_to_tms ()
{
	shared_ptr<Job> j (new SCPDCPJob (state_copy (), log(), shared_ptr<Job> ()));
	JobManager::instance()->add (j);
}

void
Film::copy_from_dvd ()
{
	shared_ptr<Job> j (new CopyFromDVDJob (state_copy (), log(), shared_ptr<Job> ()));
	j->Finished.connect (sigc::mem_fun (*this, &Film::copy_from_dvd_post_gui));
	JobManager::instance()->add (j);
}

/** Count the number of frames that have been encoded for this film.
 *  @return frame count.
 */
int
Film::encoded_frames () const
{
	if (format() == 0) {
		return 0;
	}

	int N = 0;
	for (filesystem::directory_iterator i = filesystem::directory_iterator (j2k_dir ()); i != filesystem::directory_iterator(); ++i) {
		++N;
		this_thread::interruption_point ();
	}

	return N;
}

/** Return the filename of a subtitle image if one exists for a given thumb index.
 *  @param Thumbnail index.
 *  @return Position of the image within the source frame, and the image filename, if one exists.
 *  Otherwise the filename will be empty.
 */
pair<Position, string>
Film::thumb_subtitle (int n) const
{
	string sub_file = thumb_base(n) + ".sub";
	if (!filesystem::exists (sub_file)) {
		return pair<Position, string> ();
	}

	pair<Position, string> sub;
	
	ifstream f (sub_file.c_str ());
	multimap<string, string> kv = read_key_value (f);
	for (map<string, string>::const_iterator i = kv.begin(); i != kv.end(); ++i) {
		if (i->first == "x") {
			sub.first.x = lexical_cast<int> (i->second);
		} else if (i->first == "y") {
			sub.first.y = lexical_cast<int> (i->second);
			sub.second = String::compose ("%1.sub.png", thumb_base(n));
		}
	}
	
	return sub;
}

void
Film::set_content (string c)
{
	FilmState::set_content (c);
	examine_content ();
}
