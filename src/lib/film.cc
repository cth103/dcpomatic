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
#include <libxml++/libxml++.h>
#include <libcxml/cxml.h>
#include "film.h"
#include "format.h"
#include "job.h"
#include "filter.h"
#include "util.h"
#include "job_manager.h"
#include "ab_transcode_job.h"
#include "transcode_job.h"
#include "scp_dcp_job.h"
#include "log.h"
#include "exceptions.h"
#include "examine_content_job.h"
#include "scaler.h"
#include "config.h"
#include "version.h"
#include "ui_signaller.h"
#include "analyse_audio_job.h"
#include "playlist.h"
#include "player.h"
#include "ffmpeg_content.h"
#include "imagemagick_content.h"
#include "sndfile_content.h"
#include "dcp_content_type.h"

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
using std::cout;
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
	: _playlist (new Playlist)
	, _use_dci_name (true)
	, _trust_content_headers (true)
	, _dcp_content_type (Config::instance()->default_dcp_content_type ())
	, _format (Config::instance()->default_format ())
	, _scaler (Scaler::from_id ("bicubic"))
	, _trim_start (0)
	, _trim_end (0)
	, _trim_type (CPL)
	, _ab (false)
	, _audio_gain (0)
	, _audio_delay (0)
	, _with_subtitles (false)
	, _subtitle_offset (0)
	, _subtitle_scale (1)
	, _colour_lut (0)
	, _j2k_bandwidth (200000000)
	, _dci_metadata (Config::instance()->default_dci_metadata ())
	, _dcp_video_frame_rate (0)
	, _dirty (false)
{
	set_dci_date_today ();

	_playlist->Changed.connect (bind (&Film::playlist_changed, this));
	_playlist->ContentChanged.connect (bind (&Film::playlist_content_changed, this, _1, _2));
	
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
	} else {
		write_metadata ();
	}

	_log.reset (new FileLog (file ("log")));
}

Film::Film (Film const & o)
	: boost::enable_shared_from_this<Film> (o)
	/* note: the copied film shares the original's log */
	, _log               (o._log)
	, _playlist          (new Playlist (o._playlist))
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
	, _trim_type         (o._trim_type)
	, _ab                (o._ab)
	, _audio_gain        (o._audio_gain)
	, _audio_delay       (o._audio_delay)
	, _with_subtitles    (o._with_subtitles)
	, _subtitle_offset   (o._subtitle_offset)
	, _subtitle_scale    (o._subtitle_scale)
	, _colour_lut        (o._colour_lut)
	, _j2k_bandwidth     (o._j2k_bandwidth)
	, _dci_metadata      (o._dci_metadata)
	, _dcp_video_frame_rate (o._dcp_video_frame_rate)
	, _dci_date          (o._dci_date)
	, _dirty             (o._dirty)
{
	_playlist->ContentChanged.connect (bind (&Film::playlist_content_changed, this, _1, _2));
}

string
Film::video_state_identifier () const
{
	assert (format ());
	LocaleGuard lg;

	pair<string, string> f = Filter::ffmpeg_strings (filters());

	stringstream s;
	s << format()->id()
	  << "_" << _playlist->video_digest()
	  << "_" << crop().left << "_" << crop().right << "_" << crop().top << "_" << crop().bottom
	  << "_" << _dcp_video_frame_rate
	  << "_" << f.first << "_" << f.second
	  << "_" << scaler()->id()
	  << "_" << j2k_bandwidth()
	  << "_" << boost::lexical_cast<int> (colour_lut());

	if (ab()) {
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
	p /= _playlist->audio_digest();
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
	
	log()->log (String::compose ("DCP-o-matic %1 git %2 using %3", dcpomatic_version, dcpomatic_git_commit, dependency_version_summary()));

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
#ifdef DCPOMATIC_DEBUG
	log()->log ("DCP-o-matic built in debug mode.");
#else
	log()->log ("DCP-o-matic built in optimised mode.");
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

	if (_playlist->regions().empty ()) {
		throw StringError (_("You must add some content to the DCP before creating it"));
	}

	if (dcp_content_type() == 0) {
		throw MissingSettingError (_("content type"));
	}

	if (name().empty()) {
		throw MissingSettingError (_("name"));
	}

	shared_ptr<Job> r;

	if (ab()) {
		r = JobManager::instance()->add (shared_ptr<Job> (new ABTranscodeJob (shared_from_this())));
	} else {
		r = JobManager::instance()->add (shared_ptr<Job> (new TranscodeJob (shared_from_this())));
	}
}

/** Start a job to analyse the audio in our Playlist */
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

	xmlpp::Document doc;
	xmlpp::Element* root = doc.create_root_node ("Metadata");

	root->add_child("Version")->add_child_text (boost::lexical_cast<string> (state_version));
	root->add_child("Name")->add_child_text (_name);
	root->add_child("UseDCIName")->add_child_text (_use_dci_name ? "1" : "0");
	root->add_child("TrustContentHeaders")->add_child_text (_trust_content_headers ? "1" : "0");

	if (_dcp_content_type) {
		root->add_child("DCPContentType")->add_child_text (_dcp_content_type->dci_name ());
	}

	if (_format) {
		root->add_child("Format")->add_child_text (_format->id ());
	}

	switch (_trim_type) {
	case CPL:
		root->add_child("TrimType")->add_child_text ("CPL");
		break;
	case ENCODE:
		root->add_child("TrimType")->add_child_text ("Encode");
	}
			
	root->add_child("LeftCrop")->add_child_text (boost::lexical_cast<string> (_crop.left));
	root->add_child("RightCrop")->add_child_text (boost::lexical_cast<string> (_crop.right));
	root->add_child("TopCrop")->add_child_text (boost::lexical_cast<string> (_crop.top));
	root->add_child("BottomCrop")->add_child_text (boost::lexical_cast<string> (_crop.bottom));

	for (vector<Filter const *>::const_iterator i = _filters.begin(); i != _filters.end(); ++i) {
		root->add_child("Filter")->add_child_text ((*i)->id ());
	}
	
	root->add_child("Scaler")->add_child_text (_scaler->id ());
	root->add_child("TrimStart")->add_child_text (boost::lexical_cast<string> (_trim_start));
	root->add_child("TrimEnd")->add_child_text (boost::lexical_cast<string> (_trim_end));
	root->add_child("AB")->add_child_text (_ab ? "1" : "0");
	root->add_child("AudioGain")->add_child_text (boost::lexical_cast<string> (_audio_gain));
	root->add_child("AudioDelay")->add_child_text (boost::lexical_cast<string> (_audio_delay));
	root->add_child("WithSubtitles")->add_child_text (_with_subtitles ? "1" : "0");
	root->add_child("SubtitleOffset")->add_child_text (boost::lexical_cast<string> (_subtitle_offset));
	root->add_child("SubtitleScale")->add_child_text (boost::lexical_cast<string> (_subtitle_scale));
	root->add_child("ColourLUT")->add_child_text (boost::lexical_cast<string> (_colour_lut));
	root->add_child("J2KBandwidth")->add_child_text (boost::lexical_cast<string> (_j2k_bandwidth));
	_dci_metadata.as_xml (root->add_child ("DCIMetadata"));
	root->add_child("DCPVideoFrameRate")->add_child_text (boost::lexical_cast<string> (_dcp_video_frame_rate));
	root->add_child("DCIDate")->add_child_text (boost::gregorian::to_iso_string (_dci_date));
	_playlist->as_xml (root->add_child ("Playlist"));

	doc.write_to_file_formatted (file ("metadata.xml"));
	
	_dirty = false;
}

/** Read state from our metadata file */
void
Film::read_metadata ()
{
	boost::mutex::scoped_lock lm (_state_mutex);
	LocaleGuard lg;

	if (boost::filesystem::exists (file ("metadata")) && !boost::filesystem::exists (file ("metadata.xml"))) {
		throw StringError (_("This film was created with an older version of DCP-o-matic, and unfortunately it cannot be loaded into this version.  You will need to create a new Film, re-add your content and set it up again.  Sorry!"));
	}

	cxml::File f (file ("metadata.xml"), "Metadata");
	
	_name = f.string_child ("Name");
	_use_dci_name = f.bool_child ("UseDCIName");
	_trust_content_headers = f.bool_child ("TrustContentHeaders");

	{
		optional<string> c = f.optional_string_child ("DCPContentType");
		if (c) {
			_dcp_content_type = DCPContentType::from_dci_name (c.get ());
		}
	}

	{
		optional<string> c = f.optional_string_child ("Format");
		if (c) {
			_format = Format::from_id (c.get ());
		}
	}

	{
		optional<string> c = f.optional_string_child ("TrimType");
		if (!c || c.get() == "CPL") {
			_trim_type = CPL;
		} else if (c && c.get() == "Encode") {
			_trim_type = ENCODE;
		}
	}

	_crop.left = f.number_child<int> ("LeftCrop");
	_crop.right = f.number_child<int> ("RightCrop");
	_crop.top = f.number_child<int> ("TopCrop");
	_crop.bottom = f.number_child<int> ("BottomCrop");

	{
		list<shared_ptr<cxml::Node> > c = f.node_children ("Filter");
		for (list<shared_ptr<cxml::Node> >::iterator i = c.begin(); i != c.end(); ++i) {
			_filters.push_back (Filter::from_id ((*i)->content ()));
		}
	}

	_scaler = Scaler::from_id (f.string_child ("Scaler"));
	_trim_start = f.number_child<int> ("TrimStart");
	_trim_end = f.number_child<int> ("TrimEnd");
	_ab = f.bool_child ("AB");
	_audio_gain = f.number_child<float> ("AudioGain");
	_audio_delay = f.number_child<int> ("AudioDelay");
	_with_subtitles = f.bool_child ("WithSubtitles");
	_subtitle_offset = f.number_child<float> ("SubtitleOffset");
	_subtitle_scale = f.number_child<float> ("SubtitleScale");
	_colour_lut = f.number_child<int> ("ColourLUT");
	_j2k_bandwidth = f.number_child<int> ("J2KBandwidth");
	_dci_metadata = DCIMetadata (f.node_child ("DCIMetadata"));
	_dcp_video_frame_rate = f.number_child<int> ("DCPVideoFrameRate");
	_dci_date = boost::gregorian::from_undelimited_string (f.string_child ("DCIDate"));

	_playlist->set_from_xml (f.node_child ("Playlist"));

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

	d << "_51_2K";

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

	
	Playlist::RegionList regions = _playlist->regions ();
	if (!_trust_content_headers && !regions.empty()) {
		/* We just said that we don't trust the content's header */
		for (Playlist::RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
			examine_content (i->content);
		}
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
Film::set_ab (bool a)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_ab = a;
	}
	signal_changed (AB);
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
Film::set_dcp_video_frame_rate (int f)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_dcp_video_frame_rate = f;
	}
	signal_changed (DCP_VIDEO_FRAME_RATE);
}

void
Film::signal_changed (Property p)
{
	{
		boost::mutex::scoped_lock lm (_state_mutex);
		_dirty = true;
	}

	switch (p) {
	case Film::CONTENT:
		set_dcp_video_frame_rate (_playlist->best_dcp_frame_rate ());
		break;
	default:
		break;
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

shared_ptr<Player>
Film::player () const
{
	boost::mutex::scoped_lock lm (_state_mutex);
	return shared_ptr<Player> (new Player (shared_from_this (), _playlist));
}

shared_ptr<Playlist>
Film::playlist () const
{
	boost::mutex::scoped_lock lm (_state_mutex);
	return _playlist;
}

Playlist::RegionList
Film::regions () const
{
	return _playlist->regions ();
}

void
Film::add_content (shared_ptr<Content> c)
{
	_playlist->add (c);
	examine_content (c);
}

void
Film::remove_content (shared_ptr<Content> c)
{
	_playlist->remove (c);
}

Time
Film::length () const
{
	return _playlist->length (shared_from_this ());
}

bool
Film::has_subtitles () const
{
	return _playlist->has_subtitles ();
}

OutputVideoFrame
Film::best_dcp_video_frame_rate () const
{
	return _playlist->best_dcp_frame_rate ();
}

void
Film::playlist_content_changed (boost::weak_ptr<Content> c, int p)
{
	if (p == VideoContentProperty::VIDEO_FRAME_RATE) {
		set_dcp_video_frame_rate (_playlist->best_dcp_frame_rate ());
	} 

	if (ui_signaller) {
		ui_signaller->emit (boost::bind (boost::ref (ContentChanged), c, p));
	}
}

void
Film::playlist_changed ()
{
	signal_changed (CONTENT);
}	

int
Film::loop () const
{
	return _playlist->loop ();
}

void
Film::set_loop (int c)
{
	_playlist->set_loop (c);
}

OutputAudioFrame
Film::time_to_audio_frames (Time t) const
{
	return t * dcp_audio_frame_rate () / TIME_HZ;
}

OutputVideoFrame
Film::time_to_video_frames (Time t) const
{
	return t * dcp_video_frame_rate () / TIME_HZ;
}

Time
Film::audio_frames_to_time (OutputAudioFrame f) const
{
	return f * TIME_HZ / dcp_audio_frame_rate ();
}

Time
Film::video_frames_to_time (OutputVideoFrame f) const
{
	return f * TIME_HZ / dcp_video_frame_rate ();
}

OutputAudioFrame
Film::dcp_audio_frame_rate () const
{
	/* XXX */
	return 48000;
}
