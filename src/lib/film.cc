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
#include <fstream>
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <unistd.h>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include "film.h"
#include "format.h"
#include "tiff_encoder.h"
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
	: _dirty (false)
{
	/* Make _state.directory a complete path without ..s (where possible)
	   (Code swiped from Adam Bowen on stackoverflow)
	*/
	
	filesystem::path p (filesystem::system_complete (d));
	filesystem::path result;
	for(filesystem::path::iterator i = p.begin(); i != p.end(); ++i) {
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

	_state.directory = result.string ();
	
	if (must_exist && !filesystem::exists (_state.directory)) {
		throw OpenFileError (_state.directory);
	}

	read_metadata ();

	_log = new Log (_state.file ("log"));
}

/** Copy constructor */
Film::Film (Film const & other)
	: _state (other._state)
	, _dirty (other._dirty)
{

}

Film::~Film ()
{
	delete _log;
}
	  
/** Read the `metadata' file inside this Film's directory, and fill the
 *  object's data with its content.
 */

void
Film::read_metadata ()
{
	ifstream f (metadata_file().c_str ());
	string line;
	while (getline (f, line)) {
		if (line.empty ()) {
			continue;
		}
		
		if (line[0] == '#') {
			continue;
		}

		size_t const s = line.find (' ');
		if (s == string::npos) {
			continue;
		}

		_state.read_metadata (line.substr (0, s), line.substr (s + 1));
	}

	_dirty = false;
}

/** Write our state to a file `metadata' inside the Film's directory */
void
Film::write_metadata () const
{
	filesystem::create_directories (_state.directory);
	
	ofstream f (metadata_file().c_str ());
	if (!f.good ()) {
		throw CreateFileError (metadata_file ());
	}

	_state.write_metadata (f);

	_dirty = false;
}

/** Set the name by which DVD-o-matic refers to this Film */
void
Film::set_name (string n)
{
	_state.name = n;
	signal_changed (NAME);
}

/** Set the content file for this film.
 *  @param c New content file; if specified as an absolute path, the content should
 *  be within the film's _state.directory; if specified as a relative path, the content
 *  will be assumed to be within the film's _state.directory.
 */
void
Film::set_content (string c)
{
	if (filesystem::path(c).has_root_directory () && starts_with (c, _state.directory)) {
		c = c.substr (_state.directory.length() + 1);
	}

	if (c == _state.content) {
		return;
	}
	
	/* Create a temporary decoder so that we can get information
	   about the content.
	*/
	shared_ptr<FilmState> s = state_copy ();
	s->content = c;
	shared_ptr<Options> o (new Options ("", "", ""));
	o->out_size = Size (1024, 1024);

	shared_ptr<Decoder> d = decoder_factory (s, o, 0, _log);
	
	_state.size = d->native_size ();
	_state.length = d->length_in_frames ();
	_state.frames_per_second = d->frames_per_second ();
	_state.audio_channels = d->audio_channels ();
	_state.audio_sample_rate = d->audio_sample_rate ();
	_state.audio_sample_format = d->audio_sample_format ();

	_state.content_digest = md5_digest (c);
	_state.content = c;
	
	signal_changed (SIZE);
	signal_changed (LENGTH);
	signal_changed (FRAMES_PER_SECOND);
	signal_changed (AUDIO_CHANNELS);
	signal_changed (AUDIO_SAMPLE_RATE);
	signal_changed (CONTENT);
}

/** Set the format that this Film should be shown in */
void
Film::set_format (Format const * f)
{
	_state.format = f;
	signal_changed (FORMAT);
}

/** Set the type to specify the DCP as having
 *  (feature, trailer etc.)
 */
void
Film::set_dcp_content_type (DCPContentType const * t)
{
	_state.dcp_content_type = t;
	signal_changed (DCP_CONTENT_TYPE);
}

/** Set the number of pixels by which to crop the left of the source video */
void
Film::set_left_crop (int c)
{
	if (c == _state.crop.left) {
		return;
	}
	
	_state.crop.left = c;
	signal_changed (CROP);
}

/** Set the number of pixels by which to crop the right of the source video */
void
Film::set_right_crop (int c)
{
	if (c == _state.crop.right) {
		return;
	}

	_state.crop.right = c;
	signal_changed (CROP);
}

/** Set the number of pixels by which to crop the top of the source video */
void
Film::set_top_crop (int c)
{
	if (c == _state.crop.top) {
		return;
	}
	
	_state.crop.top = c;
	signal_changed (CROP);
}

/** Set the number of pixels by which to crop the bottom of the source video */
void
Film::set_bottom_crop (int c)
{
	if (c == _state.crop.bottom) {
		return;
	}
	
	_state.crop.bottom = c;
	signal_changed (CROP);
}

/** Set the filters to apply to the image when generating thumbnails
 *  or a DCP.
 */
void
Film::set_filters (vector<Filter const *> const & f)
{
	_state.filters = f;
	signal_changed (FILTERS);
}

/** Set the number of frames to put in any generated DCP (from
 *  the start of the film).  0 indicates that all frames should
 *  be used.
 */
void
Film::set_dcp_frames (int n)
{
	_state.dcp_frames = n;
	signal_changed (DCP_FRAMES);
}

void
Film::set_dcp_trim_action (TrimAction a)
{
	_state.dcp_trim_action = a;
	signal_changed (DCP_TRIM_ACTION);
}

/** Set whether or not to generate a A/B comparison DCP.
 *  Such a DCP has the left half of its frame as the Film
 *  content without any filtering or post-processing; the
 *  right half is rendered with filters and post-processing.
 */
void
Film::set_dcp_ab (bool a)
{
	_state.dcp_ab = a;
	signal_changed (DCP_AB);
}

void
Film::set_audio_gain (float g)
{
	_state.audio_gain = g;
	signal_changed (AUDIO_GAIN);
}

void
Film::set_audio_delay (int d)
{
	_state.audio_delay = d;
	signal_changed (AUDIO_DELAY);
}

/** @return path of metadata file */
string
Film::metadata_file () const
{
	return _state.file ("metadata");
}

/** @return full path of the content (actual video) file
 *  of this Film.
 */
string
Film::content () const
{
	return _state.content_path ();
}

/** The pre-processing GUI part of a thumbs update.
 *  Must be called from the GUI thread.
 */
void
Film::update_thumbs_pre_gui ()
{
	_state.thumbs.clear ();
	filesystem::remove_all (_state.dir ("thumbs"));

	/* This call will recreate the directory */
	_state.dir ("thumbs");
}

/** The post-processing GUI part of a thumbs update.
 *  Must be called from the GUI thread.
 */
void
Film::update_thumbs_post_gui ()
{
	string const tdir = _state.dir ("thumbs");
	
	for (filesystem::directory_iterator i = filesystem::directory_iterator (tdir); i != filesystem::directory_iterator(); ++i) {

		/* Aah, the sweet smell of progress */
#if BOOST_FILESYSTEM_VERSION == 3		
		string const l = filesystem::path(*i).leaf().generic_string();
#else
		string const l = i->leaf ();
#endif
		
		size_t const d = l.find (".tiff");
		if (d != string::npos) {
			_state.thumbs.push_back (atoi (l.substr (0, d).c_str()));
		}
	}

	sort (_state.thumbs.begin(), _state.thumbs.end());
	
	write_metadata ();
	signal_changed (THUMBS);
}

/** @return the number of thumbnail images that we have */
int
Film::num_thumbs () const
{
	return _state.thumbs.size ();
}

/** @param n A thumb index.
 *  @return The frame within the Film that it is for.
 */
int
Film::thumb_frame (int n) const
{
	return _state.thumb_frame (n);
}

/** @param n A thumb index.
 *  @return The path to the thumb's image file.
 */
string
Film::thumb_file (int n) const
{
	return _state.thumb_file (n);
}

/** @return The path to the directory to write JPEG2000 files to */
string
Film::j2k_dir () const
{
	assert (format());

	filesystem::path p;


	/* Start with j2c */
	p /= "j2c";

	pair<string, string> f = Filter::ffmpeg_strings (filters ());

	/* Write stuff to specify the filter / post-processing settings that are in use,
	   so that we don't get confused about J2K files generated using different
	   settings.
	*/
	stringstream s;
	s << _state.format->id()
	  << "_" << _state.content_digest
	  << "_" << crop().left << "_" << crop().right << "_" << crop().top << "_" << crop().bottom
	  << "_" << f.first << "_" << f.second
	  << "_" << _state.scaler->id();

	p /= s.str ();

	/* Similarly for the A/B case */
	if (dcp_ab()) {
		stringstream s;
		pair<string, string> fa = Filter::ffmpeg_strings (Config::instance()->reference_filters());
		s << "ab_" << Config::instance()->reference_scaler()->id() << "_" << fa.first << "_" << fa.second;
		p /= s.str ();
	}
	
	return _state.dir (p.string ());
}

/** Handle a change to the Film's metadata */
void
Film::signal_changed (Property p)
{
	_dirty = true;
	Changed (p);
}

/** Add suitable Jobs to the JobManager to create a DCP for this Film.
 *  @param true to transcode, false to use the WAV and J2K files that are already there.
 */
void
Film::make_dcp (bool transcode, int freq)
{
	string const t = name ();
	if (t.find ("/") != string::npos) {
		throw BadSettingError ("name", "cannot contain slashes");
	}
	
	{
		stringstream s;
		s << "DVD-o-matic " << DVDOMATIC_VERSION << " using " << dependency_version_summary ();
		log()->log (s.str ());
	}

	{
		char buffer[128];
		gethostname (buffer, sizeof (buffer));
		stringstream s;
		s << "Starting to make a DCP on " << buffer;
		log()->log (s.str ());
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
	shared_ptr<Options> o (new Options (j2k_dir(), ".j2c", _state.dir ("wavs")));
	o->out_size = format()->dcp_size ();
	if (dcp_frames() == 0) {
		/* Decode the whole film, no blacking */
		o->num_frames = 0;
		o->black_after = 0;
	} else {
		switch (dcp_trim_action()) {
		case CUT:
			/* Decode only part of the film, no blacking */
			o->num_frames = dcp_frames ();
			o->black_after = 0;
			break;
		case BLACK_OUT:
			/* Decode the whole film, but black some frames out */
			o->num_frames = 0;
			o->black_after = dcp_frames ();
		}
	}
	
	o->decode_video_frequency = freq;
	o->padding = format()->dcp_padding ();
	o->ratio = format()->ratio_as_float ();

	if (transcode) {
		if (_state.dcp_ab) {
			JobManager::instance()->add (shared_ptr<Job> (new ABTranscodeJob (fs, o, log ())));
		} else {
			JobManager::instance()->add (shared_ptr<Job> (new TranscodeJob (fs, o, log ())));
		}
	}
	
	JobManager::instance()->add (shared_ptr<Job> (new MakeDCPJob (fs, o, log ())));
}

shared_ptr<FilmState>
Film::state_copy () const
{
	return shared_ptr<FilmState> (new FilmState (_state));
}

void
Film::copy_from_dvd_post_gui ()
{
	const string dvd_dir = _state.dir ("dvd");

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

void
Film::examine_content ()
{
	if (_examine_content_job) {
		return;
	}
	
	_examine_content_job.reset (new ExamineContentJob (state_copy (), log ()));
	_examine_content_job->Finished.connect (sigc::mem_fun (*this, &Film::examine_content_post_gui));
	JobManager::instance()->add (_examine_content_job);
}

void
Film::examine_content_post_gui ()
{
	_state.length = _examine_content_job->last_video_frame ();
	signal_changed (LENGTH);
	
	_examine_content_job.reset ();
}

void
Film::set_scaler (Scaler const * s)
{
	_state.scaler = s;
	signal_changed (SCALER);
}

void
Film::set_frames_per_second (float f)
{
	_state.frames_per_second = f;
	signal_changed (FRAMES_PER_SECOND);
}

/** @return full paths to any audio files that this Film has */
vector<string>
Film::audio_files () const
{
	vector<string> f;
	for (filesystem::directory_iterator i = filesystem::directory_iterator (_state.dir("wavs")); i != filesystem::directory_iterator(); ++i) {
		f.push_back (i->path().string ());
	}

	return f;
}

ContentType
Film::content_type () const
{
	return _state.content_type ();
}

void
Film::set_still_duration (int d)
{
	_state.still_duration = d;
	signal_changed (STILL_DURATION);
}

void
Film::send_dcp_to_tms ()
{
	shared_ptr<Job> j (new SCPDCPJob (state_copy (), log ()));
	JobManager::instance()->add (j);
}

void
Film::copy_from_dvd ()
{
	shared_ptr<Job> j (new CopyFromDVDJob (state_copy (), log ()));
	j->Finished.connect (sigc::mem_fun (*this, &Film::copy_from_dvd_post_gui));
	JobManager::instance()->add (j);
}

