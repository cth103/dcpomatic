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
#include "job.h"
#include "filter.h"
#include "transcoder.h"
#include "util.h"
#include "job_manager.h"
#include "ab_transcode_job.h"
#include "transcode_job.h"
#include "scp_dcp_job.h"
#include "log.h"
#include "exceptions.h"
#include "examine_content_job.h"
#include "scaler.h"
#include "decoder_factory.h"
#include "config.h"
#include "version.h"
#include "ui_signaller.h"
#include "video_decoder.h"
#include "audio_decoder.h"
#include "sndfile_decoder.h"
#include "analyse_audio_job.h"
#include "playlist.h"

#include "i18n.h"

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
using std::make_pair;
using std::endl;
using boost::shared_ptr;
using boost::lexical_cast;
using boost::to_upper_copy;
using boost::ends_with;
using boost::starts_with;
using boost::optional;
using libdcp::Size;

int const Film::state_version = 4;

/** Construct a Film object in a given directory, reading any metadata
 *  file that exists in that directory.  An exception will be thrown if
 *  must_exist is true and the specified directory does not exist.
 *
 *  @param d Film directory.
 *  @param must_exist true to throw an exception if does not exist.
 */

Film::Film (string d, bool must_exist)
	: _use_dci_name (true)
	, _trust_content_headers (true)
	, _dcp_content_type (0)
	, _format (0)
	, _scaler (Scaler::from_id ("bicubic"))
	, _trim_start (0)
	, _trim_end (0)
	, _dcp_ab (false)
	, _audio_gain (0)
	, _audio_delay (0)
	, _with_subtitles (false)
	, _subtitle_offset (0)
	, _subtitle_scale (1)
	, _colour_lut (0)
	, _j2k_bandwidth (200000000)
	, _dci_metadata (Config::instance()->default_dci_metadata ())
	, _dcp_frame_rate (0)
	, _dirty (false)
{
	set_dci_date_today ();
	
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

	if (must_exist) {
		read_metadata ();
	}

	_log.reset (new FileLog (file ("log")));
}

Film::Film (Film const & o)
	: boost::enable_shared_from_this<Film> (o)
	/* note: the copied film shares the original's log */
	, _log               (o._log)
	, _directory         (o._directory)
	, _name              (o._name)
	, _use_dci_name      (o._use_dci_name)
	, _trust_content_headers (o._trust_content_headers)
	, _dcp_content_type  (o._dcp_content_type)
	, _format            (o._format)
	, _crop              (o._crop)
	, _filters           (o._filters)
	, _scaler            (o._scaler)
	, _trim_start        (o._trim_start)
	, _trim_end          (o._trim_end)
	, _dcp_ab            (o._dcp_ab)
	, _audio_gain        (o._audio_gain)
	, _audio_delay       (o._audio_delay)
	, _with_subtitles    (o._with_subtitles)
	, _subtitle_offset   (o._subtitle_offset)
	, _subtitle_scale    (o._subtitle_scale)
	, _colour_lut        (o._colour_lut)
	, _j2k_bandwidth     (o._j2k_bandwidth)
	, _dci_metadata      (o._dci_metadata)
	, _dci_date          (o._dci_date)
	, _dcp_frame_rate    (o._dcp_frame_rate)
	, _dirty             (o._dirty)
{
	
}

Film::~Film ()
{

}

string
Film::video_state_identifier () const
{
	assert (format ());

	return "XXX";

#if 0	

	pair<string, string> f = Filter::ffmpeg_strings (filters());

	stringstream s;
	s << format()->id()
	  << "_" << content_digest()
	  << "_" << crop().left << "_" << crop().right << "_" << crop().top << "_" << crop().bottom
	  << "_" << _dcp_frame_rate
	  << "_" << f.first << "_" << f.second
	  << "_" << scaler()->id()
	  << "_" << j2k_bandwidth()
	  << "_" << boost::lexical_cast<int> (colour_lut());

	if (dcp_ab()) {
		pair<string, string> fa = Filter::ffmpeg_strings (Config::instance()->reference_filters());
		s << "ab_" << Config::instance()->reference_scaler()->id() << "_" << fa.first << "_" << fa.second;
	}

	return s.str ();
#endif	
}
	  
/** @return The path to the directory to write video frame info files to */
string
Film::info_dir () const
{
	boost::filesystem::path p;
	p /= "info";
	p /= video_state_identifier ();
	return dir (p.string());
}

string
Film::video_mxf_dir () const
{
	boost::filesystem::path p;
	return dir ("video");
}

string
Film::video_mxf_filename () const
{
	return video_state_identifier() + ".mxf";
}

string
Film::audio_analysis_path () const
{
	boost::filesystem::path p;
	p /= "analysis";
	p /= "XXX";//content_digest();
	return file (p.string ());
}

/** Add suitable Jobs to the JobManager to create a DCP for this Film */
void
Film::make_dcp ()
{
	set_dci_date_today ();
	
	if (dcp_name().find ("/") != string::npos) {
		throw BadSettingError (_("name"), _("cannot contain slashes"));
	}
	
	log()->log (String::compose ("DVD-o-matic %1 git %2 using %3", dvdomatic_version, dvdomatic_git_commit, dependency_version_summary()));

	{
		char buffer[128];
		gethostname (buffer, sizeof (buffer));
		log()->log (String::compose ("Starting to make DCP on %1", buffer));
	}
	
//	log()->log (String::compose ("Content is %1; type %2", content_path(), (content_type() == STILL ? _("still") : _("video"))));
//	if (length()) {
//		log()->log (String::compose ("Content length %1", length().get()));
//	}
//	log()->log (String::compose ("Content digest %1", content_digest()));
//	log()->log (String::compose ("Content at %1 fps, DCP at %2 fps", source_frame_rate(), dcp_frame_rate()));
	log()->log (String::compose ("%1 threads", Config::instance()->num_local_encoding_threads()));
	log()->log (String::compose ("J2K bandwidth %1", j2k_bandwidth()));
#ifdef DVDOMATIC_DEBUG
	log()->log ("DVD-o-matic built in debug mode.");
#else
	log()->log ("DVD-o-matic built in optimised mode.");
#endif
#ifdef LIBDCP_DEBUG
	log()->log ("libdcp built in debug mode.");
#else
	log()->log ("libdcp built in optimised mode.");
#endif
	pair<string, int> const c = cpu_info ();
	log()->log (String::compose ("CPU: %1, %2 processors", c.first, c.second));
	
	if (format() == 0) {
		throw MissingSettingError (_("format"));
	}

	if (content().empty ()) {
		throw MissingSettingError (_("content"));
	}

	if (dcp_content_type() == 0) {
		throw MissingSettingError (_("content type"));
	}

	if (name().empty()) {
		throw MissingSettingError (_("name"));
	}

	shared_ptr<Job> r;

	if (dcp_ab()) {
		r = JobManager::instance()->add (shared_ptr<Job> (new ABTranscodeJob (shared_from_this())));
	} else {
		r = JobManager::instance()->add (shared_ptr<Job> (new TranscodeJob (shared_from_this())));
	}
}

/** Start a job to analyse the audio of our content file */
void
Film::analyse_audio ()
{
	if (_analyse_audio_job) {
		return;
	}

	_analyse_audio_job.reset (new AnalyseAudioJob (shared_from_this()));
	_analyse_audio_job->Finished.connect (bind (&Film::analyse_audio_finished, this));
	JobManager::instance()->add (_analyse_audio_job);
}

/** Start a job to examine a piece of content */
void
Film::examine_content (shared_ptr<Content> c)
{
	shared_ptr<Job> j (new ExamineContentJob (shared_from_this(), c, trust_content_headers ()));
	j->Finished.connect (bind (&Film::examine_content_finished, this));
	JobManager::instance()->add (j);
}

void
Film::analyse_audio_finished ()
{
	ensure_ui_thread ();

	if (_analyse_audio_job->finished_ok ()) {
		AudioAnalysisSucceeded ();
	}
	
	_analyse_audio_job.reset ();
}

void
Film::examine_content_finished ()
{
	/* XXX */
}

/** Start a job to send our DCP to the configured TMS */
void
Film::send_dcp_to_tms ()
{
	shared_ptr<Job> j (new SCPDCPJob (shared_from_this()));
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
	for (boost::filesystem::directory_iterator i = boost::filesystem::directory_iterator (info_dir ()); i != boost::filesystem::directory_iterator(); ++i) {
		++N;
		boost::this_thread::interruption_point ();
	}

	return N;
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

	f << "version " << state_version << endl;

	/* User stuff */
	f << "name " << _name << endl;
	f << "use_dci_name " << _use_dci_name << endl;
//	f << "content " << _content << endl;
	f << "trust_content_headers " << (_trust_content_headers ? "1" : "0") << endl;
	if (_dcp_content_type) {
		f << "dcp_content_type " << _dcp_content_type->dci_name () << endl;
	}
	if (_format) {
		f << "format " << _format->as_metadata () << endl;
	}
	f << "left_crop " << _crop.left << endl;
	f << "right_crop " << _crop.right << endl;
	f << "top_crop " << _crop.top << endl;
	f << "bottom_crop " << _crop.bottom << endl;
	for (vector<Filter const *>::const_iterator i = _filters.begin(); i != _filters.end(); ++i) {
		f << "filter " << (*i)->id () << endl;
	}
	f << "scaler " << _scaler->id () << endl;
	f << "trim_start " << _trim_start << endl;
	f << "trim_end " << _trim_end << endl;
	f << "dcp_ab " << (_dcp_ab ? "1" : "0") << endl;
//	if (_content_audio_stream) {
//		f << "selected_content_audio_stream " << _content_audio_stream->to_string() << endl;
//	}
//	for (vector<string>::const_iterator i = _external_audio.begin(); i != _external_audio.end(); ++i) {
//		f << "external_audio " << *i << endl;
//	}
//	f << "use_content_audio " << (_use_content_audio ? "1" : "0") << endl;
	f << "audio_gain " << _audio_gain << endl;
	f << "audio_delay " << _audio_delay << endl;
//	f << "still_duration " << _still_duration << endl;
//	if (_subtitle_stream) {
//		f << "selected_subtitle_stream " << _subtitle_stream->to_string() << endl;
//	}
	f << "with_subtitles " << _with_subtitles << endl;
	f << "subtitle_offset " << _subtitle_offset << endl;
	f << "subtitle_scale " << _subtitle_scale << endl;
	f << "colour_lut " << _colour_lut << endl;
	f << "j2k_bandwidth " << _j2k_bandwidth << endl;
	_dci_metadata.write (f);
	f << "dci_date " << boost::gregorian::to_iso_string (_dci_date) << endl;
	f << "dcp_frame_rate " << _dcp_frame_rate << endl;
//	f << "width " << _size.width << endl;
//	f << "height " << _size.height << endl;
//	f << "length " << _length.get_value_or(0) << endl;
//	f << "content_digest " << _content_digest << endl;

//	for (vector<shared_ptr<AudioStream> >::const_iterator i = _content_audio_streams.begin(); i != _content_audio_streams.end(); ++i) {
//		f << "content_audio_stream " << (*i)->to_string () << endl;
//	}

//	f << "external_audio_stream " << _sndfile_stream->to_string() << endl;

//	for (vector<shared_ptr<SubtitleStream> >::const_iterator i = _subtitle_streams.begin(); i != _subtitle_streams.end(); ++i) {
//		f << "subtitle_stream " << (*i)->to_string () << endl;
//	}

//	f << "source_frame_rate " << _source_frame_rate << endl;
	
	_dirty = false;
}

/** Read state from our metadata file */
void
Film::read_metadata ()
{
	boost::mutex::scoped_lock lm (_state_mutex);

//	_external_audio.clear ();
//	_content_audio_streams.clear ();
//	_subtitle_streams.clear ();

	boost::optional<int> version;

	/* Backward compatibility things */
	boost::optional<int> audio_sample_rate;
	boost::optional<int> audio_stream_index;
	boost::optional<int> subtitle_stream_index;

	ifstream f (file ("metadata").c_str());
	if (!f.good()) {
		throw OpenFileError (file ("metadata"));
	}
	
	multimap<string, string> kv = read_key_value (f);

	/* We need version before anything else */
	multimap<string, string>::iterator v = kv.find ("version");
	if (v != kv.end ()) {
		version = atoi (v->second.c_str());
	}
	
	for (multimap<string, string>::const_iterator i = kv.begin(); i != kv.end(); ++i) {
		string const k = i->first;
		string const v = i->second;

		if (k == "audio_sample_rate") {
			audio_sample_rate = atoi (v.c_str());
		}

		/* User-specified stuff */
		if (k == "name") {
			_name = v;
		} else if (k == "use_dci_name") {
			_use_dci_name = (v == "1");
		} else if (k == "content") {
//			_content = v;
		} else if (k == "trust_content_headers") {
			_trust_content_headers = (v == "1");
		} else if (k == "dcp_content_type") {
			if (version < 3) {
				_dcp_content_type = DCPContentType::from_pretty_name (v);
			} else {
				_dcp_content_type = DCPContentType::from_dci_name (v);
			}
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
		} else if ( ((!version || version < 2) && k == "dcp_trim_start") || k == "trim_start") {
			_trim_start = atoi (v.c_str ());
		} else if ( ((!version || version < 2) && k == "dcp_trim_end") || k == "trim_end") {
			_trim_end = atoi (v.c_str ());
		} else if (k == "dcp_ab") {
			_dcp_ab = (v == "1");
		} else if (k == "selected_content_audio_stream" || (!version && k == "selected_audio_stream")) {
			if (!version) {
				audio_stream_index = atoi (v.c_str ());
			} else {
//				_content_audio_stream = audio_stream_factory (v, version);
			}
		} else if (k == "external_audio") {
//			_external_audio.push_back (v);
		} else if (k == "use_content_audio") {
//			_use_content_audio = (v == "1");
		} else if (k == "audio_gain") {
			_audio_gain = atof (v.c_str ());
		} else if (k == "audio_delay") {
			_audio_delay = atoi (v.c_str ());
		} else if (k == "still_duration") {
//			_still_duration = atoi (v.c_str ());
		} else if (k == "selected_subtitle_stream") {
			if (!version) {
				subtitle_stream_index = atoi (v.c_str ());
			} else {
//				_subtitle_stream = subtitle_stream_factory (v, version);
			}
		} else if (k == "with_subtitles") {
			_with_subtitles = (v == "1");
		} else if (k == "subtitle_offset") {
			_subtitle_offset = atoi (v.c_str ());
		} else if (k == "subtitle_scale") {
			_subtitle_scale = atof (v.c_str ());
		} else if (k == "colour_lut") {
			_colour_lut = atoi (v.c_str ());
		} else if (k == "j2k_bandwidth") {
			_j2k_bandwidth = atoi (v.c_str ());
		} else if (k == "dci_date") {
			_dci_date = boost::gregorian::from_undelimited_string (v);
		} else if (k == "dcp_frame_rate") {
			_dcp_frame_rate = atoi (v.c_str ());
		}

		_dci_metadata.read (k, v);
		
		/* Cached stuff */
		if (k == "width") {
//			_size.width = atoi (v.c_str ());
		} else if (k == "height") {
//			_size.height = atoi (v.c_str ());
		} else if (k == "length") {
			int const vv = atoi (v.c_str ());
			if (vv) {
//				_length = vv;
			}
		} else if (k == "content_digest") {
//			_content_digest = v;
		} else if (k == "content_audio_stream" || (!version && k == "audio_stream")) {
//			_content_audio_streams.push_back (audio_stream_factory (v, version));
		} else if (k == "external_audio_stream") {
//			_sndfile_stream = audio_stream_factory (v, version);
		} else if (k == "subtitle_stream") {
//			_subtitle_streams.push_back (subtitle_stream_factory (v, version));
		} else if (k == "source_frame_rate") {
//			_source_frame_rate = atof (v.c_str ());
		} else if (version < 4 && k == "frames_per_second") {
//			_source_frame_rate = atof (v.c_str ());
			/* Fill in what would have been used for DCP frame rate by the older version */
//			_dcp_frame_rate = best_dcp_frame_rate (_source_frame_rate);
		}
	}

	_dirty = false;
}

libdcp::Size
Film::cropped_size (libdcp::Size s) const
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
 *  Any required parent directories will be created.
 */
string
Film::file (string f) const
{
	boost::mutex::scoped_lock lm (_directory_mutex);

	boost::filesystem::path p;
	p /= _directory;
	p /= f;

	boost::filesystem::create_directories (p.parent_path ());
	
	return p.string ();
}

/** @return The sampling rate that we will resample the audio to */
int
Film::target_audio_sample_rate () const
{
	/* XXX: how often is this method called? */
	
	boost::shared_ptr<Playlist> p = playlist ();
	if (p->has_audio ()) {
		return 0;
	}
	
	/* Resample to a DCI-approved sample rate */
	double t = dcp_audio_sample_rate (p->audio_frame_rate());

	FrameRateConversion frc (p->video_frame_rate(), dcp_frame_rate());

	/* Compensate if the DCP is being run at a different frame rate
	   to the source; that is, if the video is run such that it will
	   look different in the DCP compared to the source (slower or faster).
	   skip/repeat doesn't come into effect here.
	*/

	if (frc.change_speed) {
		t *= p->video_frame_rate() * frc.factor() / dcp_frame_rate();
	}

	return rint (t);
}

/** @return a DCI-compliant name for a DCP of this film */
string
Film::dci_name (bool if_created_now) const
{
	stringstream d;

	string fixed_name = to_upper_copy (name());
	for (size_t i = 0; i < fixed_name.length(); ++i) {
		if (fixed_name[i] == ' ') {
			fixed_name[i] = '-';
		}
	}

	/* Spec is that the name part should be maximum 14 characters, as I understand it */
	if (fixed_name.length() > 14) {
		fixed_name = fixed_name.substr (0, 14);
	}

	d << fixed_name;

	if (dcp_content_type()) {
		d << "_" << dcp_content_type()->dci_name();
	}

	if (format()) {
		d << "_" << format()->dci_name();
	}

	DCIMetadata const dm = dci_metadata ();

	if (!dm.audio_language.empty ()) {
		d << "_" << dm.audio_language;
		if (!dm.subtitle_language.empty()) {
			d << "-" << dm.subtitle_language;
		} else {
			d << "-XX";
		}
	}

	if (!dm.territory.empty ()) {
		d << "_" << dm.territory;
		if (!dm.rating.empty ()) {
			d << "-" << dm.rating;
		}
	}

	/* XXX */
	switch (2) {
	case 1:
		d << "_10";
		break;
	case 2:
		d << "_20";
		break;
	case 6:
		d << "_51";
		break;
	case 8:
		d << "_71";
		break;
	}

	d << "_2K";

	if (!dm.studio.empty ()) {
		d << "_" << dm.studio;
	}

	if (if_created_now) {
		d << "_" << boost::gregorian::to_iso_string (boost::gregorian::day_clock::local_day ());
	} else {
		d << "_" << boost::gregorian::to_iso_string (_dci_date);
	}

	if (!dm.facility.empty ()) {
		d << "_" << dm.facility;
	}

	if (!dm.package_type.empty ()) {
		d << "_" << dm.package_type;
	}

	return d.str ();
}

/** @return name to give the DCP */
string
Film::dcp_name (bool if_created_now) const
{
	if (use_dci_name()) {
		return dci_name (if_created_now);
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
Film::set_trust_content_headers (bool t)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_trust_content_headers = t;
	}
	
	signal_changed (TRUST_CONTENT_HEADERS);

	if (!_trust_content_headers && !content().empty()) {
		/* We just said that we don't trust the content's header */
		/* XXX */
//		examine_content ();
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
Film::set_trim_start (int t)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_trim_start = t;
	}
	signal_changed (TRIM_START);
}

void
Film::set_trim_end (int t)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_trim_end = t;
	}
	signal_changed (TRIM_END);
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
Film::set_colour_lut (int i)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_colour_lut = i;
	}
	signal_changed (COLOUR_LUT);
}

void
Film::set_j2k_bandwidth (int b)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_j2k_bandwidth = b;
	}
	signal_changed (J2K_BANDWIDTH);
}

void
Film::set_dci_metadata (DCIMetadata m)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_dci_metadata = m;
	}
	signal_changed (DCI_METADATA);
}


void
Film::set_dcp_frame_rate (int f)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_dcp_frame_rate = f;
	}
	signal_changed (DCP_FRAME_RATE);
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

void
Film::set_dci_date_today ()
{
	_dci_date = boost::gregorian::day_clock::local_day ();
}

string
Film::info_path (int f) const
{
	boost::filesystem::path p;
	p /= info_dir ();

	stringstream s;
	s.width (8);
	s << setfill('0') << f << ".md5";

	p /= s.str();

	/* info_dir() will already have added any initial bit of the path,
	   so don't call file() on this.
	*/
	return p.string ();
}

string
Film::j2c_path (int f, bool t) const
{
	boost::filesystem::path p;
	p /= "j2c";
	p /= video_state_identifier ();

	stringstream s;
	s.width (8);
	s << setfill('0') << f << ".j2c";

	if (t) {
		s << ".tmp";
	}

	p /= s.str();
	return file (p.string ());
}

/** Make an educated guess as to whether we have a complete DCP
 *  or not.
 *  @return true if we do.
 */

bool
Film::have_dcp () const
{
	try {
		libdcp::DCP dcp (dir (dcp_name()));
		dcp.read ();
	} catch (...) {
		return false;
	}

	return true;
}

shared_ptr<Playlist>
Film::playlist () const
{
	boost::mutex::scoped_lock lm (_state_mutex);
	return shared_ptr<Playlist> (new Playlist (shared_from_this (), _content));
}

void
Film::add_content (shared_ptr<Content> c)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_content.push_back (c);
	}

	signal_changed (CONTENT);

	examine_content (c);
}
