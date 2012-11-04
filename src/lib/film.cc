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
#include <boost/date_time.hpp>
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
#include "log.h"
#include "options.h"
#include "exceptions.h"
#include "examine_content_job.h"
#include "scaler.h"
#include "decoder_factory.h"
#include "config.h"
#include "check_hashes_job.h"
#include "version.h"
#include "ui_signaller.h"

using std::string;
using std::stringstream;
using std::multimap;
using std::pair;
using std::map;
using std::vector;
using std::ifstream;
using std::ofstream;
using std::setfill;
using std::min;
using boost::shared_ptr;
using boost::lexical_cast;
using boost::to_upper_copy;
using boost::ends_with;
using boost::starts_with;

/** Construct a Film object in a given directory, reading any metadata
 *  file that exists in that directory.  An exception will be thrown if
 *  must_exist is true and the specified directory does not exist.
 *
 *  @param d Film directory.
 *  @param must_exist true to throw an exception if does not exist.
 */

Film::Film (string d, bool must_exist)
	: _use_dci_name (false)
	, _dcp_content_type (0)
	, _format (0)
	, _scaler (Scaler::from_id ("bicubic"))
	, _dcp_trim_start (0)
	, _dcp_trim_end (0)
	, _dcp_ab (false)
	, _audio_stream (-1)
	, _audio_gain (0)
	, _audio_delay (0)
	, _still_duration (10)
	, _subtitle_stream (-1)
	, _with_subtitles (false)
	, _subtitle_offset (0)
	, _subtitle_scale (1)
	, _audio_sample_rate (0)
	, _has_subtitles (false)
	, _frames_per_second (0)
	, _dirty (false)
{
	/* Make state.directory a complete path without ..s (where possible)
	   (Code swiped from Adam Bowen on stackoverflow)
	*/
	
	boost::filesystem::path p (boost::filesystem::system_complete (d));
	boost::filesystem::path result;
	for (boost::filesystem::path::iterator i = p.begin(); i != p.end(); ++i) {
		if (*i == "..") {
			if (boost::filesystem::is_symlink (result) || result.filename() == "..") {
				result /= *i;
			} else {
				result = result.parent_path ();
			}
		} else if (*i != ".") {
			result /= *i;
		}
	}

	set_directory (result.string ());
	
	if (!boost::filesystem::exists (directory())) {
		if (must_exist) {
			throw OpenFileError (directory());
		} else {
			boost::filesystem::create_directory (directory());
		}
	}

	read_metadata ();

	_log = new FileLog (file ("log"));
	set_dci_date_today ();
}

Film::Film (Film const & o)
	: _log (0)
	, _directory         (o._directory)
	, _name              (o._name)
	, _use_dci_name      (o._use_dci_name)
	, _content           (o._content)
	, _dcp_content_type  (o._dcp_content_type)
	, _format            (o._format)
	, _crop              (o._crop)
	, _filters           (o._filters)
	, _scaler            (o._scaler)
	, _dcp_trim_start    (o._dcp_trim_start)
	, _dcp_trim_end      (o._dcp_trim_end)
	, _dcp_ab            (o._dcp_ab)
	, _audio_stream      (o._audio_stream)
	, _audio_gain        (o._audio_gain)
	, _audio_delay       (o._audio_delay)
	, _still_duration    (o._still_duration)
	, _subtitle_stream   (o._subtitle_stream)
	, _with_subtitles    (o._with_subtitles)
	, _subtitle_offset   (o._subtitle_offset)
	, _subtitle_scale    (o._subtitle_scale)
	, _audio_language    (o._audio_language)
	, _subtitle_language (o._subtitle_language)
	, _territory         (o._territory)
	, _rating            (o._rating)
	, _studio            (o._studio)
	, _facility          (o._facility)
	, _package_type      (o._package_type)
	, _thumbs            (o._thumbs)
	, _size              (o._size)
	, _length            (o._length)
	, _audio_sample_rate (o._audio_sample_rate)
	, _content_digest    (o._content_digest)
	, _has_subtitles     (o._has_subtitles)
	, _audio_streams     (o._audio_streams)
	, _subtitle_streams  (o._subtitle_streams)
	, _frames_per_second (o._frames_per_second)
	, _dirty             (o._dirty)
{

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

	boost::filesystem::path p;

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
	set_dci_date_today ();
	
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

	shared_ptr<Options> o (new Options (j2k_dir(), ".j2c", dir ("wavs")));
	o->out_size = format()->dcp_size ();
	o->padding = format()->dcp_padding (shared_from_this ());
	o->ratio = format()->ratio_as_float (shared_from_this ());
	o->decode_subtitles = with_subtitles ();
	o->decode_video_skip = dcp_frame_rate (frames_per_second()).skip;

	shared_ptr<Job> r;

	if (transcode) {
		if (dcp_ab()) {
			r = JobManager::instance()->add (shared_ptr<Job> (new ABTranscodeJob (shared_from_this(), o, shared_ptr<Job> ())));
		} else {
			r = JobManager::instance()->add (shared_ptr<Job> (new TranscodeJob (shared_from_this(), o, shared_ptr<Job> ())));
		}
	}

	r = JobManager::instance()->add (shared_ptr<Job> (new CheckHashesJob (shared_from_this(), o, r)));
	JobManager::instance()->add (shared_ptr<Job> (new MakeDCPJob (shared_from_this(), o, r)));
}

/** Start a job to examine our content file */
void
Film::examine_content ()
{
	if (_examine_content_job) {
		return;
	}

	set_thumbs (vector<int> ());
	boost::filesystem::remove_all (dir ("thumbs"));

	/* This call will recreate the directory */
	dir ("thumbs");
	
	_examine_content_job.reset (new ExamineContentJob (shared_from_this(), shared_ptr<Job> ()));
	_examine_content_job->Finished.connect (bind (&Film::examine_content_finished, this));
	JobManager::instance()->add (_examine_content_job);
}

void
Film::examine_content_finished ()
{
	_examine_content_job.reset ();
}

/** @return full paths to any audio files that this Film has */
vector<string>
Film::audio_files () const
{
	vector<string> f;
	for (boost::filesystem::directory_iterator i = boost::filesystem::directory_iterator (dir("wavs")); i != boost::filesystem::directory_iterator(); ++i) {
		f.push_back (i->path().string ());
	}

	return f;
}

/** Start a job to send our DCP to the configured TMS */
void
Film::send_dcp_to_tms ()
{
	shared_ptr<Job> j (new SCPDCPJob (shared_from_this(), shared_ptr<Job> ()));
	JobManager::instance()->add (j);
}

void
Film::copy_from_dvd ()
{
	shared_ptr<Job> j (new CopyFromDVDJob (shared_from_this(), shared_ptr<Job> ()));
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
	for (boost::filesystem::directory_iterator i = boost::filesystem::directory_iterator (j2k_dir ()); i != boost::filesystem::directory_iterator(); ++i) {
		++N;
		boost::this_thread::interruption_point ();
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
	if (!boost::filesystem::exists (sub_file)) {
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

/** Write state to our `metadata' file */
void
Film::write_metadata () const
{
	boost::mutex::scoped_lock lm (_state_mutex);

	boost::filesystem::create_directories (directory());

	string const m = file ("metadata");
	ofstream f (m.c_str ());
	if (!f.good ()) {
		throw CreateFileError (m);
	}

	/* User stuff */
	f << "name " << _name << "\n";
	f << "use_dci_name " << _use_dci_name << "\n";
	f << "content " << _content << "\n";
	if (_dcp_content_type) {
		f << "dcp_content_type " << _dcp_content_type->pretty_name () << "\n";
	}
	if (_format) {
		f << "format " << _format->as_metadata () << "\n";
	}
	f << "left_crop " << _crop.left << "\n";
	f << "right_crop " << _crop.right << "\n";
	f << "top_crop " << _crop.top << "\n";
	f << "bottom_crop " << _crop.bottom << "\n";
	for (vector<Filter const *>::const_iterator i = _filters.begin(); i != _filters.end(); ++i) {
		f << "filter " << (*i)->id () << "\n";
	}
	f << "scaler " << _scaler->id () << "\n";
	f << "dcp_trim_start " << _dcp_trim_start << "\n";
	f << "dcp_trim_end " << _dcp_trim_end << "\n";
	f << "dcp_ab " << (_dcp_ab ? "1" : "0") << "\n";
	f << "selected_audio_stream " << _audio_stream << "\n";
	f << "audio_gain " << _audio_gain << "\n";
	f << "audio_delay " << _audio_delay << "\n";
	f << "still_duration " << _still_duration << "\n";
	f << "selected_subtitle_stream " << _subtitle_stream << "\n";
	f << "with_subtitles " << _with_subtitles << "\n";
	f << "subtitle_offset " << _subtitle_offset << "\n";
	f << "subtitle_scale " << _subtitle_scale << "\n";
	f << "audio_language " << _audio_language << "\n";
	f << "subtitle_language " << _subtitle_language << "\n";
	f << "territory " << _territory << "\n";
	f << "rating " << _rating << "\n";
	f << "studio " << _studio << "\n";
	f << "facility " << _facility << "\n";
	f << "package_type " << _package_type << "\n";

	/* Cached stuff; this is information about our content; we could
	   look it up each time, but that's slow.
	*/
	for (vector<int>::const_iterator i = _thumbs.begin(); i != _thumbs.end(); ++i) {
		f << "thumb " << *i << "\n";
	}
	f << "width " << _size.width << "\n";
	f << "height " << _size.height << "\n";
	f << "length " << _length.get_value_or(0) << "\n";
	f << "audio_sample_rate " << _audio_sample_rate << "\n";
	f << "content_digest " << _content_digest << "\n";
	f << "has_subtitles " << _has_subtitles << "\n";

	for (vector<AudioStream>::const_iterator i = _audio_streams.begin(); i != _audio_streams.end(); ++i) {
		f << "audio_stream " << i->to_string () << "\n";
	}

	for (vector<SubtitleStream>::const_iterator i = _subtitle_streams.begin(); i != _subtitle_streams.end(); ++i) {
		f << "subtitle_stream " << i->to_string () << "\n";
	}

	f << "frames_per_second " << _frames_per_second << "\n";
	
	_dirty = false;
}

/** Read state from our metadata file */
void
Film::read_metadata ()
{
	boost::mutex::scoped_lock lm (_state_mutex);
	
	ifstream f (file ("metadata").c_str());
	multimap<string, string> kv = read_key_value (f);
	for (multimap<string, string>::const_iterator i = kv.begin(); i != kv.end(); ++i) {
		string const k = i->first;
		string const v = i->second;

		/* User-specified stuff */
		if (k == "name") {
			_name = v;
		} else if (k == "use_dci_name") {
			_use_dci_name = (v == "1");
		} else if (k == "content") {
			_content = v;
		} else if (k == "dcp_content_type") {
			_dcp_content_type = DCPContentType::from_pretty_name (v);
		} else if (k == "format") {
			_format = Format::from_metadata (v);
		} else if (k == "left_crop") {
			_crop.left = atoi (v.c_str ());
		} else if (k == "right_crop") {
			_crop.right = atoi (v.c_str ());
		} else if (k == "top_crop") {
			_crop.top = atoi (v.c_str ());
		} else if (k == "bottom_crop") {
			_crop.bottom = atoi (v.c_str ());
		} else if (k == "filter") {
			_filters.push_back (Filter::from_id (v));
		} else if (k == "scaler") {
			_scaler = Scaler::from_id (v);
		} else if (k == "dcp_trim_start") {
			_dcp_trim_start = atoi (v.c_str ());
		} else if (k == "dcp_trim_end") {
			_dcp_trim_end = atoi (v.c_str ());
		} else if (k == "dcp_ab") {
			_dcp_ab = (v == "1");
		} else if (k == "selected_audio_stream") {
			_audio_stream = atoi (v.c_str ());
		} else if (k == "audio_gain") {
			_audio_gain = atof (v.c_str ());
		} else if (k == "audio_delay") {
			_audio_delay = atoi (v.c_str ());
		} else if (k == "still_duration") {
			_still_duration = atoi (v.c_str ());
		} else if (k == "selected_subtitle_stream") {
			_subtitle_stream = atoi (v.c_str ());
		} else if (k == "with_subtitles") {
			_with_subtitles = (v == "1");
		} else if (k == "subtitle_offset") {
			_subtitle_offset = atoi (v.c_str ());
		} else if (k == "subtitle_scale") {
			_subtitle_scale = atof (v.c_str ());
		} else if (k == "audio_language") {
			_audio_language = v;
		} else if (k == "subtitle_language") {
			_subtitle_language = v;
		} else if (k == "territory") {
			_territory = v;
		} else if (k == "rating") {
			_rating = v;
		} else if (k == "studio") {
			_studio = v;
		} else if (k == "facility") {
			_facility = v;
		} else if (k == "package_type") {
			_package_type = v;
		}
		
		/* Cached stuff */
		if (k == "thumb") {
			int const n = atoi (v.c_str ());
			/* Only add it to the list if it still exists */
			if (boost::filesystem::exists (thumb_file_for_frame (n))) {
				_thumbs.push_back (n);
			}
		} else if (k == "width") {
			_size.width = atoi (v.c_str ());
		} else if (k == "height") {
			_size.height = atoi (v.c_str ());
		} else if (k == "length") {
			int const vv = atoi (v.c_str ());
			if (vv) {
				_length = vv;
			}
		} else if (k == "audio_sample_rate") {
			_audio_sample_rate = atoi (v.c_str ());
		} else if (k == "content_digest") {
			_content_digest = v;
		} else if (k == "has_subtitles") {
			_has_subtitles = (v == "1");
		} else if (k == "audio_stream") {
			_audio_streams.push_back (AudioStream (v));
		} else if (k == "subtitle_stream") {
			_subtitle_streams.push_back (SubtitleStream (v));
		} else if (k == "frames_per_second") {
			_frames_per_second = atof (v.c_str ());
		}
	}
		
	_dirty = false;
}

/** @param n A thumb index.
 *  @return The path to the thumb's image file.
 */
string
Film::thumb_file (int n) const
{
	return thumb_file_for_frame (thumb_frame (n));
}

/** @param n A frame index within the Film.
 *  @return The path to the thumb's image file for this frame;
 *  we assume that it exists.
 */
string
Film::thumb_file_for_frame (int n) const
{
	return thumb_base_for_frame(n) + ".png";
}

/** Must not be called with the _state_mutex locked */
string
Film::thumb_base (int n) const
{
	return thumb_base_for_frame (thumb_frame (n));
}

string
Film::thumb_base_for_frame (int n) const
{
	stringstream s;
	s.width (8);
	s << setfill('0') << n;
	
	boost::filesystem::path p;
	p /= dir ("thumbs");
	p /= s.str ();
		
	return p.string ();
}

/** @param n A thumb index.
 *  @return The frame within the Film that it is for.
 *
 *  Must not be called with the _state_mutex locked.
 */
int
Film::thumb_frame (int n) const
{
	boost::mutex::scoped_lock lm (_state_mutex);
	assert (n < int (_thumbs.size ()));
	return _thumbs[n];
}

Size
Film::cropped_size (Size s) const
{
	boost::mutex::scoped_lock lm (_state_mutex);
	s.width -= _crop.left + _crop.right;
	s.height -= _crop.top + _crop.bottom;
	return s;
}

/** Given a directory name, return its full path within the Film's directory.
 *  The directory (and its parents) will be created if they do not exist.
 */
string
Film::dir (string d) const
{
	boost::mutex::scoped_lock lm (_directory_mutex);
	boost::filesystem::path p;
	p /= _directory;
	p /= d;
	boost::filesystem::create_directories (p);
	return p.string ();
}

/** Given a file or directory name, return its full path within the Film's directory.
 *  _directory_mutex must not be locked on entry.
 */
string
Film::file (string f) const
{
	boost::mutex::scoped_lock lm (_directory_mutex);
	boost::filesystem::path p;
	p /= _directory;
	p /= f;
	return p.string ();
}

/** @return full path of the content (actual video) file
 *  of the Film.
 */
string
Film::content_path () const
{
	boost::mutex::scoped_lock lm (_state_mutex);
	if (boost::filesystem::path(_content).has_root_directory ()) {
		return _content;
	}

	return file (_content);
}

ContentType
Film::content_type () const
{
#if BOOST_FILESYSTEM_VERSION == 3
	string ext = boost::filesystem::path(_content).extension().string();
#else
	string ext = boost::filesystem::path(_content).extension();
#endif

	transform (ext.begin(), ext.end(), ext.begin(), ::tolower);
	
	if (ext == ".tif" || ext == ".tiff" || ext == ".jpg" || ext == ".jpeg" || ext == ".png") {
		return STILL;
	}

	return VIDEO;
}

/** @return The sampling rate that we will resample the audio to */
int
Film::target_audio_sample_rate () const
{
	/* Resample to a DCI-approved sample rate */
	double t = dcp_audio_sample_rate (audio_sample_rate());

	DCPFrameRate dfr = dcp_frame_rate (frames_per_second ());

	/* Compensate for the fact that video will be rounded to the
	   nearest integer number of frames per second.
	*/
	if (dfr.run_fast) {
		t *= _frames_per_second * dfr.skip / dfr.frames_per_second;
	}

	return rint (t);
}

boost::optional<int>
Film::dcp_length () const
{
	if (!length()) {
		return boost::optional<int> ();
	}

	return length().get() - dcp_trim_start() - dcp_trim_end();
}

/** @return a DCI-compliant name for a DCP of this film */
string
Film::dci_name () const
{
	boost::mutex::scoped_lock lm (_state_mutex);
	
	stringstream d;

	string fixed_name = to_upper_copy (_name);
	for (size_t i = 0; i < fixed_name.length(); ++i) {
		if (fixed_name[i] == ' ') {
			fixed_name[i] = '-';
		}
	}

	/* Spec is that the name part should be maximum 14 characters, as I understand it */
	if (fixed_name.length() > 14) {
		fixed_name = fixed_name.substr (0, 14);
	}

	d << fixed_name << "_";

	if (_dcp_content_type) {
		d << _dcp_content_type->dci_name() << "_";
	}

	if (_format) {
		d << _format->dci_name() << "_";
	}

	if (!_audio_language.empty ()) {
		d << _audio_language;
		if (!_subtitle_language.empty() && _with_subtitles) {
			d << "-" << _subtitle_language;
		} else {
			d << "-XX";
		}
			
		d << "_";
	}

	if (!_territory.empty ()) {
		d << _territory;
		if (!_rating.empty ()) {
			d << "-" << _rating;
		}
		d << "_";
	}

	switch (_audio_streams[_audio_stream].channels()) {
	case 1:
		d << "10_";
		break;
	case 2:
		d << "20_";
		break;
	case 6:
		d << "51_";
		break;
	case 8:
		d << "71_";
		break;
	}

	d << "2K_";

	if (!_studio.empty ()) {
		d << _studio << "_";
	}

	d << boost::gregorian::to_iso_string (_dci_date) << "_";

	if (!_facility.empty ()) {
		d << _facility << "_";
	}

	if (!_package_type.empty ()) {
		d << _package_type;
	}

	return d.str ();
}

/** @return name to give the DCP */
string
Film::dcp_name () const
{
	if (use_dci_name()) {
		return dci_name ();
	}

	return name();
}


void
Film::set_directory (string d)
{
	boost::mutex::scoped_lock lm (_state_mutex);
	_directory = d;
	_dirty = true;
}

void
Film::set_name (string n)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_name = n;
	}
	signal_changed (NAME);
}

void
Film::set_use_dci_name (bool u)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_use_dci_name = u;
	}
	signal_changed (USE_DCI_NAME);
}

void
Film::set_content (string c)
{
	string check = directory ();

#if BOOST_FILESYSTEM_VERSION == 3
	boost::filesystem::path slash ("/");
	string platform_slash = slash.make_preferred().string ();
#else
#ifdef DVDOMATIC_WINDOWS
	string platform_slash = "\\";
#else
	string platform_slash = "/";
#endif
#endif	

	if (!ends_with (check, platform_slash)) {
		check += platform_slash;
	}
	
	if (boost::filesystem::path(c).has_root_directory () && starts_with (c, check)) {
		c = c.substr (_directory.length() + 1);
	}

	string old_content;
	
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		if (c == _content) {
			return;
		}

		old_content = _content;
		_content = c;
	}

	/* Create a temporary decoder so that we can get information
	   about the content.
	*/

	try {
		shared_ptr<Options> o (new Options ("", "", ""));
		o->out_size = Size (1024, 1024);
		
		shared_ptr<Decoder> d = decoder_factory (shared_from_this(), o, 0, 0);
		
		set_size (d->native_size ());
		set_frames_per_second (d->frames_per_second ());
		set_audio_sample_rate (d->audio_sample_rate ());
		set_has_subtitles (d->has_subtitles ());
		set_audio_streams (d->audio_streams ());
		set_subtitle_streams (d->subtitle_streams ());
		set_audio_stream (audio_streams().empty() ? -1 : 0);
		set_subtitle_stream (subtitle_streams().empty() ? -1 : 0);
		
		{
			boost::mutex::scoped_lock lm (_state_mutex);
			_content = c;
		}
		
		signal_changed (CONTENT);
		
		set_content_digest (md5_digest (content_path ()));
		
		examine_content ();

	} catch (...) {

		boost::mutex::scoped_lock lm (_state_mutex);
		_content = old_content;
		throw;

	}
}
	       
void
Film::set_dcp_content_type (DCPContentType const * t)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_dcp_content_type = t;
	}
	signal_changed (DCP_CONTENT_TYPE);
}

void
Film::set_format (Format const * f)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_format = f;
	}
	signal_changed (FORMAT);
}

void
Film::set_crop (Crop c)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_crop = c;
	}
	signal_changed (CROP);
}

void
Film::set_left_crop (int c)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		
		if (_crop.left == c) {
			return;
		}
		
		_crop.left = c;
	}
	signal_changed (CROP);
}

void
Film::set_right_crop (int c)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		if (_crop.right == c) {
			return;
		}
		
		_crop.right = c;
	}
	signal_changed (CROP);
}

void
Film::set_top_crop (int c)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		if (_crop.top == c) {
			return;
		}
		
		_crop.top = c;
	}
	signal_changed (CROP);
}

void
Film::set_bottom_crop (int c)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		if (_crop.bottom == c) {
			return;
		}
		
		_crop.bottom = c;
	}
	signal_changed (CROP);
}

void
Film::set_filters (vector<Filter const *> f)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_filters = f;
	}
	signal_changed (FILTERS);
}

void
Film::set_scaler (Scaler const * s)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_scaler = s;
	}
	signal_changed (SCALER);
}

void
Film::set_dcp_trim_start (int t)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_dcp_trim_start = t;
	}
	signal_changed (DCP_TRIM_START);
}

void
Film::set_dcp_trim_end (int t)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_dcp_trim_end = t;
	}
	signal_changed (DCP_TRIM_END);
}

void
Film::set_dcp_ab (bool a)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_dcp_ab = a;
	}
	signal_changed (DCP_AB);
}

void
Film::set_audio_stream (int s)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_audio_stream = s;
	}
	signal_changed (AUDIO_STREAM);
}

void
Film::set_audio_gain (float g)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_audio_gain = g;
	}
	signal_changed (AUDIO_GAIN);
}

void
Film::set_audio_delay (int d)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_audio_delay = d;
	}
	signal_changed (AUDIO_DELAY);
}

void
Film::set_still_duration (int d)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_still_duration = d;
	}
	signal_changed (STILL_DURATION);
}

void
Film::set_subtitle_stream (int s)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_subtitle_stream = s;
	}
	signal_changed (SUBTITLE_STREAM);
}

void
Film::set_with_subtitles (bool w)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_with_subtitles = w;
	}
	signal_changed (WITH_SUBTITLES);
}

void
Film::set_subtitle_offset (int o)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_subtitle_offset = o;
	}
	signal_changed (SUBTITLE_OFFSET);
}

void
Film::set_subtitle_scale (float s)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_subtitle_scale = s;
	}
	signal_changed (SUBTITLE_SCALE);
}

void
Film::set_audio_language (string l)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_audio_language = l;
	}
	signal_changed (DCI_METADATA);
}

void
Film::set_subtitle_language (string l)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_subtitle_language = l;
	}
	signal_changed (DCI_METADATA);
}

void
Film::set_territory (string t)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_territory = t;
	}
	signal_changed (DCI_METADATA);
}

void
Film::set_rating (string r)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_rating = r;
	}
	signal_changed (DCI_METADATA);
}

void
Film::set_studio (string s)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_studio = s;
	}
	signal_changed (DCI_METADATA);
}

void
Film::set_facility (string f)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_facility = f;
	}
	signal_changed (DCI_METADATA);
}

void
Film::set_package_type (string p)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_package_type = p;
	}
	signal_changed (DCI_METADATA);
}

void
Film::set_thumbs (vector<int> t)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_thumbs = t;
	}
	signal_changed (THUMBS);
}

void
Film::set_size (Size s)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_size = s;
	}
	signal_changed (SIZE);
}

void
Film::set_length (int l)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_length = l;
	}
	signal_changed (LENGTH);
}

void
Film::unset_length ()
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_length = boost::none;
	}
	signal_changed (LENGTH);
}	

void
Film::set_audio_sample_rate (int r)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_audio_sample_rate = r;
	}
	signal_changed (AUDIO_SAMPLE_RATE);
}

void
Film::set_content_digest (string d)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_content_digest = d;
	}
	_dirty = true;
}

void
Film::set_has_subtitles (bool s)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_has_subtitles = s;
	}
	signal_changed (HAS_SUBTITLES);
}

void
Film::set_audio_streams (vector<AudioStream> s)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_audio_streams = s;
	}
	_dirty = true;
}

void
Film::set_subtitle_streams (vector<SubtitleStream> s)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_subtitle_streams = s;
	}
	_dirty = true;
}

void
Film::set_frames_per_second (float f)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_frames_per_second = f;
	}
	signal_changed (FRAMES_PER_SECOND);
}
	
void
Film::signal_changed (Property p)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_dirty = true;
	}

	if (ui_signaller) {
		ui_signaller->emit (boost::bind (boost::ref (Changed), p));
	}
}

int
Film::audio_channels () const
{
	boost::mutex::scoped_lock lm (_state_mutex);
	if (_audio_stream == -1) {
		return 0;
	}
	
	return _audio_streams[_audio_stream].channels ();
}

void
Film::set_dci_date_today ()
{
	_dci_date = boost::gregorian::day_clock::local_day ();
}
