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
#include "options.h"
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
#include "cross.h"

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
using std::list;
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
	, _trust_content_header (true)
	, _dcp_content_type (Config::instance()->default_dcp_content_type ())
	, _format (Config::instance()->default_format ())
	, _scaler (Scaler::from_id ("bicubic"))
	, _trim_start (0)
	, _trim_end (0)
	, _trim_type (CPL)
	, _dcp_ab (false)
	, _use_content_audio (true)
	, _audio_gain (0)
	, _audio_delay (0)
	, _still_duration (10)
	, _with_subtitles (false)
	, _subtitle_offset (0)
	, _subtitle_scale (1)
	, _colour_lut (0)
	, _j2k_bandwidth (200000000)
	, _dci_metadata (Config::instance()->default_dci_metadata ())
	, _dcp_frame_rate (0)
	, _source_frame_rate (0)
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

	_sndfile_stream = SndfileStream::create ();
	
	_log.reset (new FileLog (file ("log")));
	
	if (must_exist) {
		read_metadata ();
	} else {
		write_metadata ();
	}
}

Film::Film (Film const & o)
	: boost::enable_shared_from_this<Film> (o)
	/* note: the copied film shares the original's log */
	, _log               (o._log)
	, _directory         (o._directory)
	, _name              (o._name)
	, _use_dci_name      (o._use_dci_name)
	, _content           (o._content)
	, _trust_content_header (o._trust_content_header)
	, _dcp_content_type  (o._dcp_content_type)
	, _format            (o._format)
	, _crop              (o._crop)
	, _filters           (o._filters)
	, _scaler            (o._scaler)
	, _trim_start        (o._trim_start)
	, _trim_end          (o._trim_end)
	, _trim_type         (o._trim_type)
	, _dcp_ab            (o._dcp_ab)
	, _content_audio_stream (o._content_audio_stream)
	, _external_audio    (o._external_audio)
	, _use_content_audio (o._use_content_audio)
	, _audio_gain        (o._audio_gain)
	, _audio_delay       (o._audio_delay)
	, _still_duration    (o._still_duration)
	, _subtitle_stream   (o._subtitle_stream)
	, _with_subtitles    (o._with_subtitles)
	, _subtitle_offset   (o._subtitle_offset)
	, _subtitle_scale    (o._subtitle_scale)
	, _colour_lut        (o._colour_lut)
	, _j2k_bandwidth     (o._j2k_bandwidth)
	, _dci_metadata      (o._dci_metadata)
	, _dci_date          (o._dci_date)
	, _dcp_frame_rate    (o._dcp_frame_rate)
	, _size              (o._size)
	, _length            (o._length)
	, _content_digest    (o._content_digest)
	, _content_audio_streams (o._content_audio_streams)
	, _sndfile_stream    (o._sndfile_stream)
	, _subtitle_streams  (o._subtitle_streams)
	, _source_frame_rate (o._source_frame_rate)
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
	LocaleGuard lg;

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

	if (trim_type() == ENCODE) {
		s << "_" << trim_start() << "_" << trim_end();
	}

	if (dcp_ab()) {
		pair<string, string> fa = Filter::ffmpeg_strings (Config::instance()->reference_filters());
		s << "ab_" << Config::instance()->reference_scaler()->id() << "_" << fa.first << "_" << fa.second;
	}

	return s.str ();
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
Film::internal_video_mxf_dir () const
{
	boost::filesystem::path p;
	return dir ("video");
}

string
Film::internal_video_mxf_filename () const
{
	return video_state_identifier() + ".mxf";
}

string
Film::dcp_video_mxf_filename () const
{
	return filename_safe_name() + "_video.mxf";
}

string
Film::dcp_audio_mxf_filename () const
{
	return filename_safe_name() + "_audio.mxf";
}

string
Film::filename_safe_name () const
{
	string const n = name ();
	string o;
	for (size_t i = 0; i < n.length(); ++i) {
		if (isalnum (n[i])) {
			o += n[i];
		} else {
			o += "_";
		}
	}

	return o;
}

string
Film::audio_analysis_path () const
{
	boost::filesystem::path p;
	p /= "analysis";
	p /= content_digest();
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
	
	log()->log (String::compose ("Content is %1; type %2", content_path(), (content_type() == STILL ? _("still") : _("video"))));
	if (length()) {
		log()->log (String::compose ("Content length %1", length().get()));
	}
	log()->log (String::compose ("Content digest %1", content_digest()));
	log()->log (String::compose ("Content at %1 fps, DCP at %2 fps", source_frame_rate(), dcp_frame_rate()));
	log()->log (String::compose ("%1 threads", Config::instance()->num_local_encoding_threads()));
	log()->log (String::compose ("J2K bandwidth %1", j2k_bandwidth()));
	if (use_content_audio()) {
		log()->log ("Using content's audio");
	} else {
		log()->log (String::compose ("Using external audio (%1 files)", external_audio().size()));
	}
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
	list<pair<string, string> > const m = mount_info ();
	for (list<pair<string, string> >::const_iterator i = m.begin(); i != m.end(); ++i) {
		log()->log (String::compose ("Mount: %1 %2", i->first, i->second));
	}
	
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

	DecodeOptions od;
	od.decode_subtitles = with_subtitles ();

	shared_ptr<Job> r;

	if (dcp_ab()) {
		r = JobManager::instance()->add (shared_ptr<Job> (new ABTranscodeJob (shared_from_this(), od)));
	} else {
		r = JobManager::instance()->add (shared_ptr<Job> (new TranscodeJob (shared_from_this(), od)));
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

/** Start a job to examine our content file */
void
Film::examine_content ()
{
	if (_examine_content_job) {
		return;
	}

	_examine_content_job.reset (new ExamineContentJob (shared_from_this()));
	_examine_content_job->Finished.connect (bind (&Film::examine_content_finished, this));
	JobManager::instance()->add (_examine_content_job);
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
	_examine_content_job.reset ();
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
	LocaleGuard lg;

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
	f << "content " << _content << endl;
	f << "trust_content_header " << (_trust_content_header ? "1" : "0") << endl;
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
	switch (_trim_type) {
	case CPL:
		f << "trim_type cpl\n";
		break;
	case ENCODE:
		f << "trim_type encode\n";
		break;
	}
	f << "dcp_ab " << (_dcp_ab ? "1" : "0") << endl;
	if (_content_audio_stream) {
		f << "selected_content_audio_stream " << _content_audio_stream->to_string() << endl;
	}
	for (vector<string>::const_iterator i = _external_audio.begin(); i != _external_audio.end(); ++i) {
		f << "external_audio " << *i << endl;
	}
	f << "use_content_audio " << (_use_content_audio ? "1" : "0") << endl;
	f << "audio_gain " << _audio_gain << endl;
	f << "audio_delay " << _audio_delay << endl;
	f << "still_duration " << _still_duration << endl;
	if (_subtitle_stream) {
		f << "selected_subtitle_stream " << _subtitle_stream->to_string() << endl;
	}
	f << "with_subtitles " << _with_subtitles << endl;
	f << "subtitle_offset " << _subtitle_offset << endl;
	f << "subtitle_scale " << _subtitle_scale << endl;
	f << "colour_lut " << _colour_lut << endl;
	f << "j2k_bandwidth " << _j2k_bandwidth << endl;
	_dci_metadata.write (f);
	f << "dci_date " << boost::gregorian::to_iso_string (_dci_date) << endl;
	f << "dcp_frame_rate " << _dcp_frame_rate << endl;
	f << "width " << _size.width << endl;
	f << "height " << _size.height << endl;
	f << "length " << _length.get_value_or(0) << endl;
	f << "content_digest " << _content_digest << endl;

	for (vector<shared_ptr<AudioStream> >::const_iterator i = _content_audio_streams.begin(); i != _content_audio_streams.end(); ++i) {
		f << "content_audio_stream " << (*i)->to_string () << endl;
	}

	f << "external_audio_stream " << _sndfile_stream->to_string() << endl;

	for (vector<shared_ptr<SubtitleStream> >::const_iterator i = _subtitle_streams.begin(); i != _subtitle_streams.end(); ++i) {
		f << "subtitle_stream " << (*i)->to_string () << endl;
	}

	f << "source_frame_rate " << _source_frame_rate << endl;
	
	_dirty = false;
}

/** Read state from our metadata file */
void
Film::read_metadata ()
{
	boost::mutex::scoped_lock lm (_state_mutex);
	LocaleGuard lg;

	_external_audio.clear ();
	_content_audio_streams.clear ();
	_subtitle_streams.clear ();

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
			_content = v;
		} else if (k == "trust_content_header") {
			_trust_content_header = (v == "1");
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
		} else if (k == "trim_type") {
			if (v == "cpl") {
				_trim_type = CPL;
			} else if (v == "encode") {
				_trim_type = ENCODE;
			}
		} else if (k == "dcp_ab") {
			_dcp_ab = (v == "1");
		} else if (k == "selected_content_audio_stream" || (!version && k == "selected_audio_stream")) {
			if (!version) {
				audio_stream_index = atoi (v.c_str ());
			} else {
				_content_audio_stream = audio_stream_factory (v, version);
			}
		} else if (k == "external_audio") {
			_external_audio.push_back (v);
		} else if (k == "use_content_audio") {
			_use_content_audio = (v == "1");
		} else if (k == "audio_gain") {
			_audio_gain = atof (v.c_str ());
		} else if (k == "audio_delay") {
			_audio_delay = atoi (v.c_str ());
		} else if (k == "still_duration") {
			_still_duration = atoi (v.c_str ());
		} else if (k == "selected_subtitle_stream") {
			if (!version) {
				subtitle_stream_index = atoi (v.c_str ());
			} else {
				_subtitle_stream = subtitle_stream_factory (v, version);
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
			_size.width = atoi (v.c_str ());
		} else if (k == "height") {
			_size.height = atoi (v.c_str ());
		} else if (k == "length") {
			int const vv = atoi (v.c_str ());
			if (vv) {
				_length = vv;
			}
		} else if (k == "content_digest") {
			_content_digest = v;
		} else if (k == "content_audio_stream" || (!version && k == "audio_stream")) {
			_content_audio_streams.push_back (audio_stream_factory (v, version));
		} else if (k == "external_audio_stream") {
			_sndfile_stream = audio_stream_factory (v, version);
		} else if (k == "subtitle_stream") {
			_subtitle_streams.push_back (subtitle_stream_factory (v, version));
		} else if (k == "source_frame_rate") {
			_source_frame_rate = atof (v.c_str ());
		} else if (version < 4 && k == "frames_per_second") {
			_source_frame_rate = atof (v.c_str ());
			/* Fill in what would have been used for DCP frame rate by the older version */
			_dcp_frame_rate = best_dcp_frame_rate (_source_frame_rate);
		}
	}

	if (!version) {
		if (audio_sample_rate) {
			/* version < 1 didn't specify sample rate in the audio streams, so fill it in here */
			for (vector<shared_ptr<AudioStream> >::iterator i = _content_audio_streams.begin(); i != _content_audio_streams.end(); ++i) {
				(*i)->set_sample_rate (audio_sample_rate.get());
			}
		}

		/* also the selected stream was specified as an index */
		if (audio_stream_index && audio_stream_index.get() >= 0 && audio_stream_index.get() < (int) _content_audio_streams.size()) {
			_content_audio_stream = _content_audio_streams[audio_stream_index.get()];
		}

		/* similarly the subtitle */
		if (subtitle_stream_index && subtitle_stream_index.get() >= 0 && subtitle_stream_index.get() < (int) _subtitle_streams.size()) {
			_subtitle_stream = _subtitle_streams[subtitle_stream_index.get()];
		}
	}
		
	_dirty = false;

	_log->log (String::compose ("Loaded film with use_content_audio = %1", _use_content_audio));
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
	if (boost::filesystem::is_directory (_content)) {
		/* Directory of images, we assume */
		return VIDEO;
	}

	if (still_image_file (_content)) {
		return STILL;
	}

	return VIDEO;
}

/** @return The sampling rate that we will resample the audio to */
int
Film::target_audio_sample_rate () const
{
	if (!audio_stream()) {
		return 0;
	}
	
	/* Resample to a DCI-approved sample rate */
	double t = dcp_audio_sample_rate (audio_stream()->sample_rate());

	FrameRateConversion frc (source_frame_rate(), dcp_frame_rate());

	/* Compensate if the DCP is being run at a different frame rate
	   to the source; that is, if the video is run such that it will
	   look different in the DCP compared to the source (slower or faster).
	   skip/repeat doesn't come into effect here.
	*/

	if (frc.change_speed) {
		t *= source_frame_rate() * frc.factor() / dcp_frame_rate();
	}

	return rint (t);
}

int
Film::still_duration_in_frames () const
{
	return still_duration() * source_frame_rate();
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

	switch (audio_channels()) {
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
Film::set_content (string c)
{
	string check = directory ();

	boost::filesystem::path slash ("/");
	string platform_slash = slash.make_preferred().string ();

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

	/* Do this before we start using FFmpeg ourselves */
	run_ffprobe (c, file ("ffprobe.log"), _log);
	
	/* Reset streams here in case the new content doesn't have one or the other */
	_content_audio_stream = shared_ptr<AudioStream> ();
	_subtitle_stream = shared_ptr<SubtitleStream> ();

	/* Start off using content audio */
	set_use_content_audio (true);

	/* Create a temporary decoder so that we can get information
	   about the content.
	*/

	try {
		Decoders d = decoder_factory (shared_from_this(), DecodeOptions());
		
		set_size (d.video->native_size ());
		set_source_frame_rate (d.video->frames_per_second ());
		set_dcp_frame_rate (best_dcp_frame_rate (source_frame_rate ()));
		set_subtitle_streams (d.video->subtitle_streams ());
		if (d.audio) {
			set_content_audio_streams (d.audio->audio_streams ());
		}

		{
			boost::mutex::scoped_lock lm (_state_mutex);
			_content = c;
		}
		
		signal_changed (CONTENT);
		
		/* Start off with the first audio and subtitle streams */
		if (d.audio && !d.audio->audio_streams().empty()) {
			set_content_audio_stream (d.audio->audio_streams().front());
		}
		
		if (!d.video->subtitle_streams().empty()) {
			set_subtitle_stream (d.video->subtitle_streams().front());
		}
		
		examine_content ();

	} catch (...) {

		boost::mutex::scoped_lock lm (_state_mutex);
		_content = old_content;
		throw;

	}

	/* Default format */
	set_format (Config::instance()->default_format ());

	/* Still image DCPs must use external audio */
	if (content_type() == STILL) {
		set_use_content_audio (false);
	}
}

void
Film::set_trust_content_header (bool t)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_trust_content_header = t;
	}
	
	signal_changed (TRUST_CONTENT_HEADER);

	if (!_trust_content_header && !content().empty()) {
		/* We just said that we don't trust the content's header */
		examine_content ();
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
Film::set_trim_type (TrimType t)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_trim_type = t;
	}
	signal_changed (TRIM_TYPE);
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
Film::set_content_audio_stream (shared_ptr<AudioStream> s)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_content_audio_stream = s;
	}
	signal_changed (CONTENT_AUDIO_STREAM);
}

void
Film::set_external_audio (vector<string> a)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_external_audio = a;
	}

	shared_ptr<SndfileDecoder> decoder (new SndfileDecoder (shared_from_this(), DecodeOptions()));
	if (decoder->audio_stream()) {
		_sndfile_stream = decoder->audio_stream ();
	}
	
	signal_changed (EXTERNAL_AUDIO);
}

void
Film::set_use_content_audio (bool e)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_use_content_audio = e;
	}

	signal_changed (USE_CONTENT_AUDIO);
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
Film::set_subtitle_stream (shared_ptr<SubtitleStream> s)
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
Film::set_size (libdcp::Size s)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_size = s;
	}
	signal_changed (SIZE);
}

void
Film::set_length (SourceFrame l)
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
Film::set_content_digest (string d)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_content_digest = d;
	}
	_dirty = true;
}

void
Film::set_content_audio_streams (vector<shared_ptr<AudioStream> > s)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_content_audio_streams = s;
	}
	signal_changed (CONTENT_AUDIO_STREAMS);
}

void
Film::set_subtitle_streams (vector<shared_ptr<SubtitleStream> > s)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_subtitle_streams = s;
	}
	signal_changed (SUBTITLE_STREAMS);
}

void
Film::set_source_frame_rate (float f)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_source_frame_rate = f;
	}
	signal_changed (SOURCE_FRAME_RATE);
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
	shared_ptr<AudioStream> s = audio_stream ();
	if (!s) {
		return 0;
	}

	return s->channels ();
}

void
Film::set_dci_date_today ()
{
	_dci_date = boost::gregorian::day_clock::local_day ();
}

boost::shared_ptr<AudioStream>
Film::audio_stream () const
{
	if (use_content_audio()) {
		return _content_audio_stream;
	}

	return _sndfile_stream;
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

bool
Film::has_audio () const
{
	if (use_content_audio()) {
		return audio_stream();
	}

	vector<string> const e = external_audio ();
	for (vector<string>::const_iterator i = e.begin(); i != e.end(); ++i) {
		if (!i->empty ()) {
			return true;
		}
	}

	return false;
}

