/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

/** @file  src/film.cc
 *  @brief A representation of some audio and video content, and details of
 *  how they should be presented in a DCP.
 */

#include "film.h"
#include "job.h"
#include "util.h"
#include "job_manager.h"
#include "transcode_job.h"
#include "scp_dcp_job.h"
#include "log.h"
#include "exceptions.h"
#include "examine_content_job.h"
#include "scaler.h"
#include "config.h"
#include "version.h"
#include "ui_signaller.h"
#include "playlist.h"
#include "player.h"
#include "dcp_content_type.h"
#include "ratio.h"
#include "cross.h"
#include "cinema.h"
#include "safe_stringstream.h"
#include <libcxml/cxml.h>
#include <dcp/cpl.h>
#include <dcp/signer.h>
#include <dcp/util.h>
#include <dcp/local_time.h>
#include <dcp/raw_convert.h>
#include <dcp/decrypted_kdm.h>
#include <libxml++/libxml++.h>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>
#include <unistd.h>
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <cstdlib>
#include <iomanip>
#include <set>

#include "i18n.h"

using std::string;
using std::multimap;
using std::pair;
using std::map;
using std::vector;
using std::setfill;
using std::min;
using std::make_pair;
using std::endl;
using std::cout;
using std::list;
using std::set;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;
using boost::to_upper_copy;
using boost::ends_with;
using boost::starts_with;
using boost::optional;
using boost::is_any_of;
using dcp::Size;
using dcp::Signer;
using dcp::raw_convert;
using dcp::raw_convert;

#define LOG_GENERAL(...) log()->log (String::compose (__VA_ARGS__), Log::TYPE_GENERAL);
#define LOG_GENERAL_NC(...) log()->log (__VA_ARGS__, Log::TYPE_GENERAL);

/* 5 -> 6
 * AudioMapping XML changed.
 * 6 -> 7
 * Subtitle offset changed to subtitle y offset, and subtitle x offset added.
 * 7 -> 8
 * Use <Scale> tag in <VideoContent> rather than <Ratio>.
 * 8 -> 9
 * DCI -> ISDCF
 * 9 -> 10
 * Subtitle X and Y scale.
 *
 * Bumped to 32 for 2.0 branch; some times are expressed in Times rather
 * than frames now.
 */
int const Film::current_state_version = 32;

/** Construct a Film object in a given directory.
 *
 *  @param dir Film directory.
 */

Film::Film (boost::filesystem::path dir, bool log)
	: _playlist (new Playlist)
	, _use_isdcf_name (true)
	, _dcp_content_type (Config::instance()->default_dcp_content_type ())
	, _container (Config::instance()->default_container ())
	, _resolution (RESOLUTION_2K)
	, _scaler (Scaler::from_id ("bicubic"))
	, _signed (true)
	, _encrypted (false)
	, _j2k_bandwidth (Config::instance()->default_j2k_bandwidth ())
	, _isdcf_metadata (Config::instance()->default_isdcf_metadata ())
	, _video_frame_rate (24)
	, _audio_channels (6)
	, _three_d (false)
	, _sequence_video (true)
	, _interop (false)
	, _burn_subtitles (false)
	, _state_version (current_state_version)
	, _dirty (false)
{
	set_isdcf_date_today ();

	_playlist->Changed.connect (bind (&Film::playlist_changed, this));
	_playlist->ContentChanged.connect (bind (&Film::playlist_content_changed, this, _1, _2));
	
	/* Make state.directory a complete path without ..s (where possible)
	   (Code swiped from Adam Bowen on stackoverflow)
	*/
	
	boost::filesystem::path p (boost::filesystem::system_complete (dir));
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

	set_directory (result.make_preferred ());
	if (log) {
		_log.reset (new FileLog (file ("log")));
	} else {
		_log.reset (new NullLog);
	}

	_playlist->set_sequence_video (_sequence_video);
}

string
Film::video_identifier () const
{
	DCPOMATIC_ASSERT (container ());

	SafeStringStream s;
	s.imbue (std::locale::classic ());
	
	s << container()->id()
	  << "_" << resolution_to_string (_resolution)
	  << "_" << _playlist->video_identifier()
	  << "_" << _video_frame_rate
	  << "_" << scaler()->id()
	  << "_" << j2k_bandwidth();

	if (encrypted ()) {
		s << "_E";
	} else {
		s << "_P";
	}

	if (_interop) {
		s << "_I";
	} else {
		s << "_S";
	}

	if (_burn_subtitles) {
		s << "_B";
	}

	if (_three_d) {
		s << "_3D";
	}

	return s.str ();
}
	  
/** @return The path to the directory to write video frame info files to */
boost::filesystem::path
Film::info_dir () const
{
	boost::filesystem::path p;
	p /= "info";
	p /= video_identifier ();
	return dir (p);
}

boost::filesystem::path
Film::internal_video_mxf_dir () const
{
	return dir ("video");
}

boost::filesystem::path
Film::internal_video_mxf_filename () const
{
	return video_identifier() + ".mxf";
}

boost::filesystem::path
Film::video_mxf_filename () const
{
	return filename_safe_name() + "_video.mxf";
}

boost::filesystem::path
Film::audio_mxf_filename () const
{
	return filename_safe_name() + "_audio.mxf";
}

boost::filesystem::path
Film::subtitle_xml_filename () const
{
	return filename_safe_name() + "_subtitle.xml";
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

boost::filesystem::path
Film::audio_analysis_dir () const
{
	return dir ("analysis");
}

/** Add suitable Jobs to the JobManager to create a DCP for this Film */
void
Film::make_dcp ()
{
	set_isdcf_date_today ();
	
	if (dcp_name().find ("/") != string::npos) {
		throw BadSettingError (_("name"), _("cannot contain slashes"));
	}

	LOG_GENERAL ("DCP-o-matic %1 git %2 using %3", dcpomatic_version, dcpomatic_git_commit, dependency_version_summary());

	{
		char buffer[128];
		gethostname (buffer, sizeof (buffer));
		LOG_GENERAL ("Starting to make DCP on %1", buffer);
	}

	ContentList cl = content ();
	for (ContentList::const_iterator i = cl.begin(); i != cl.end(); ++i) {
		LOG_GENERAL ("Content: %1", (*i)->technical_summary());
	}
	LOG_GENERAL ("DCP video rate %1 fps", video_frame_rate());
	LOG_GENERAL ("%1 threads", Config::instance()->num_local_encoding_threads());
	LOG_GENERAL ("J2K bandwidth %1", j2k_bandwidth());
#ifdef DCPOMATIC_DEBUG
	LOG_GENERAL_NC ("DCP-o-matic built in debug mode.");
#else
	LOG_GENERAL_NC ("DCP-o-matic built in optimised mode.");
#endif
#ifdef LIBDCP_DEBUG
	LOG_GENERAL_NC ("libdcp built in debug mode.");
#else
	LOG_GENERAL_NC ("libdcp built in optimised mode.");
#endif

#ifdef DCPOMATIC_WINDOWS
	OSVERSIONINFO info;
	info.dwOSVersionInfoSize = sizeof (info);
	GetVersionEx (&info);
	LOG_GENERAL ("Windows version %1.%2.%3 SP %4", info.dwMajorVersion, info.dwMinorVersion, info.dwBuildNumber, info.szCSDVersion);
#endif	
	
	LOG_GENERAL ("CPU: %1, %2 processors", cpu_info(), boost::thread::hardware_concurrency ());
	list<pair<string, string> > const m = mount_info ();
	for (list<pair<string, string> >::const_iterator i = m.begin(); i != m.end(); ++i) {
		LOG_GENERAL ("Mount: %1 %2", i->first, i->second);
	}
	
	if (container() == 0) {
		throw MissingSettingError (_("container"));
	}

	if (content().empty()) {
		throw StringError (_("You must add some content to the DCP before creating it"));
	}

	if (dcp_content_type() == 0) {
		throw MissingSettingError (_("content type"));
	}

	if (name().empty()) {
		throw MissingSettingError (_("name"));
	}

	JobManager::instance()->add (shared_ptr<Job> (new TranscodeJob (shared_from_this())));
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
	if (container() == 0) {
		return 0;
	}

	int N = 0;
	for (boost::filesystem::directory_iterator i = boost::filesystem::directory_iterator (info_dir ()); i != boost::filesystem::directory_iterator(); ++i) {
		++N;
		boost::this_thread::interruption_point ();
	}

	return N;
}

shared_ptr<xmlpp::Document>
Film::metadata () const
{
	shared_ptr<xmlpp::Document> doc (new xmlpp::Document);
	xmlpp::Element* root = doc->create_root_node ("Metadata");

	root->add_child("Version")->add_child_text (raw_convert<string> (current_state_version));
	root->add_child("Name")->add_child_text (_name);
	root->add_child("UseISDCFName")->add_child_text (_use_isdcf_name ? "1" : "0");

	if (_dcp_content_type) {
		root->add_child("DCPContentType")->add_child_text (_dcp_content_type->isdcf_name ());
	}

	if (_container) {
		root->add_child("Container")->add_child_text (_container->id ());
	}

	root->add_child("Resolution")->add_child_text (resolution_to_string (_resolution));
	root->add_child("Scaler")->add_child_text (_scaler->id ());
	root->add_child("J2KBandwidth")->add_child_text (raw_convert<string> (_j2k_bandwidth));
	_isdcf_metadata.as_xml (root->add_child ("ISDCFMetadata"));
	root->add_child("VideoFrameRate")->add_child_text (raw_convert<string> (_video_frame_rate));
	root->add_child("ISDCFDate")->add_child_text (boost::gregorian::to_iso_string (_isdcf_date));
	root->add_child("AudioChannels")->add_child_text (raw_convert<string> (_audio_channels));
	root->add_child("ThreeD")->add_child_text (_three_d ? "1" : "0");
	root->add_child("SequenceVideo")->add_child_text (_sequence_video ? "1" : "0");
	root->add_child("Interop")->add_child_text (_interop ? "1" : "0");
	root->add_child("BurnSubtitles")->add_child_text (_burn_subtitles ? "1" : "0");
	root->add_child("Signed")->add_child_text (_signed ? "1" : "0");
	root->add_child("Encrypted")->add_child_text (_encrypted ? "1" : "0");
	root->add_child("Key")->add_child_text (_key.hex ());
	_playlist->as_xml (root->add_child ("Playlist"));

	return doc;
}

/** Write state to our `metadata' file */
void
Film::write_metadata () const
{
	boost::filesystem::create_directories (directory ());
	shared_ptr<xmlpp::Document> doc = metadata ();
	doc->write_to_file_formatted (file("metadata.xml").string ());
	_dirty = false;
}

/** Read state from our metadata file.
 *  @return Notes about things that the user should know about, or empty.
 */
list<string>
Film::read_metadata ()
{
	if (boost::filesystem::exists (file ("metadata")) && !boost::filesystem::exists (file ("metadata.xml"))) {
		throw StringError (_("This film was created with an older version of DCP-o-matic, and unfortunately it cannot be loaded into this version.  You will need to create a new Film, re-add your content and set it up again.  Sorry!"));
	}

	cxml::Document f ("Metadata");
	f.read_file (file ("metadata.xml"));

	_state_version = f.number_child<int> ("Version");
	if (_state_version > current_state_version) {
		throw StringError (_("This film was created with a newer version of DCP-o-matic, and it cannot be loaded into this version.  Sorry!"));
	}
	
	_name = f.string_child ("Name");
	if (_state_version >= 9) {
		_use_isdcf_name = f.bool_child ("UseISDCFName");
		_isdcf_metadata = ISDCFMetadata (f.node_child ("ISDCFMetadata"));
		_isdcf_date = boost::gregorian::from_undelimited_string (f.string_child ("ISDCFDate"));
	} else {
		_use_isdcf_name = f.bool_child ("UseDCIName");
		_isdcf_metadata = ISDCFMetadata (f.node_child ("DCIMetadata"));
		_isdcf_date = boost::gregorian::from_undelimited_string (f.string_child ("DCIDate"));
	}

	{
		optional<string> c = f.optional_string_child ("DCPContentType");
		if (c) {
			_dcp_content_type = DCPContentType::from_isdcf_name (c.get ());
		}
	}

	{
		optional<string> c = f.optional_string_child ("Container");
		if (c) {
			_container = Ratio::from_id (c.get ());
		}
	}

	_resolution = string_to_resolution (f.string_child ("Resolution"));
	_scaler = Scaler::from_id (f.string_child ("Scaler"));
	_j2k_bandwidth = f.number_child<int> ("J2KBandwidth");
	_video_frame_rate = f.number_child<int> ("VideoFrameRate");
	_signed = f.optional_bool_child("Signed").get_value_or (true);
	_encrypted = f.bool_child ("Encrypted");
	_audio_channels = f.number_child<int> ("AudioChannels");
	_sequence_video = f.bool_child ("SequenceVideo");
	_three_d = f.bool_child ("ThreeD");
	_interop = f.bool_child ("Interop");
	if (_state_version >= 32) {
		_burn_subtitles = f.bool_child ("BurnSubtitles");
	}
	_key = dcp::Key (f.string_child ("Key"));

	list<string> notes;
	/* This method is the only one that can return notes (so far) */
	_playlist->set_from_xml (shared_from_this(), f.node_child ("Playlist"), _state_version, notes);

	/* Write backtraces to this film's directory, until another film is loaded */
	set_backtrace_file (file ("backtrace.txt"));

	_dirty = false;
	return notes;
}

/** Given a directory name, return its full path within the Film's directory.
 *  The directory (and its parents) will be created if they do not exist.
 */
boost::filesystem::path
Film::dir (boost::filesystem::path d) const
{
	boost::filesystem::path p;
	p /= _directory;
	p /= d;
	
	boost::filesystem::create_directories (p);
	
	return p;
}

/** Given a file or directory name, return its full path within the Film's directory.
 *  Any required parent directories will be created.
 */
boost::filesystem::path
Film::file (boost::filesystem::path f) const
{
	boost::filesystem::path p;
	p /= _directory;
	p /= f;

	boost::filesystem::create_directories (p.parent_path ());
	
	return p;
}

/** @return a ISDCF-compliant name for a DCP of this film */
string
Film::isdcf_name (bool if_created_now) const
{
	SafeStringStream d;

	string raw_name = name ();

	/* Split the raw name up into words */
	vector<string> words;
	split (words, raw_name, is_any_of (" "));

	string fixed_name;
	
	/* Add each word to fixed_name */
	for (vector<string>::const_iterator i = words.begin(); i != words.end(); ++i) {
		string w = *i;

		/* First letter is always capitalised */
		w[0] = toupper (w[0]);

		/* Count caps in w */
		size_t caps = 0;
		for (size_t i = 0; i < w.size(); ++i) {
			if (isupper (w[i])) {
				++caps;
			}
		}
		
		/* If w is all caps make the rest of it lower case, otherwise
		   leave it alone.
		*/
		if (caps == w.size ()) {
			for (size_t i = 1; i < w.size(); ++i) {
				w[i] = tolower (w[i]);
			}
		}

		for (size_t i = 0; i < w.size(); ++i) {
			fixed_name += w[i];
		}
	}

	if (fixed_name.length() > 14) {
		fixed_name = fixed_name.substr (0, 14);
	}

	d << fixed_name;

	if (dcp_content_type()) {
		d << "_" << dcp_content_type()->isdcf_name();
		d << "-" << isdcf_metadata().content_version;
	}

	ISDCFMetadata const dm = isdcf_metadata ();

	if (dm.temp_version) {
		d << "-Temp";
	}
	
	if (dm.pre_release) {
		d << "-Pre";
	}
	
	if (dm.red_band) {
		d << "-RedBand";
	}
	
	if (!dm.chain.empty ()) {
		d << "-" << dm.chain;
	}

	if (three_d ()) {
		d << "-3D";
	}

	if (dm.two_d_version_of_three_d) {
		d << "-2D";
	}

	if (!dm.mastered_luminance.empty ()) {
		d << "-" << dm.mastered_luminance;
	}

	if (video_frame_rate() != 24) {
		d << "-" << video_frame_rate();
	}
	
	if (container()) {
		d << "_" << container()->isdcf_name();
	}

	/* XXX: this uses the first bit of content only */

	/* The standard says we don't do this for trailers, for some strange reason */
	if (dcp_content_type() && dcp_content_type()->libdcp_kind() != dcp::TRAILER) {
		ContentList cl = content ();
		Ratio const * content_ratio = 0;
		for (ContentList::iterator i = cl.begin(); i != cl.end(); ++i) {
			shared_ptr<VideoContent> vc = dynamic_pointer_cast<VideoContent> (*i);
			if (vc) {
				/* Here's the first piece of video content */
				if (vc->scale().ratio ()) {
					content_ratio = vc->scale().ratio ();
				} else {
					content_ratio = Ratio::from_ratio (vc->video_size().ratio ());
				}
				break;
			}
		}
		
		if (content_ratio && content_ratio != container()) {
			d << "-" << content_ratio->isdcf_name();
		}
	}

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

	switch (audio_channels ()) {
	case 1:
		d << "_10";
		break;
	case 2:
		d << "_20";
		break;
	case 3:
		d << "_30";
		break;
	case 4:
		d << "_40";
		break;
	case 5:
		d << "_50";
		break;
	case 6:
		d << "_51";
		break;
	}

	/* XXX: HI/VI */

	d << "_" << resolution_to_string (_resolution);
	
	if (!dm.studio.empty ()) {
		d << "_" << dm.studio;
	}

	if (if_created_now) {
		d << "_" << boost::gregorian::to_iso_string (boost::gregorian::day_clock::local_day ());
	} else {
		d << "_" << boost::gregorian::to_iso_string (_isdcf_date);
	}

	if (!dm.facility.empty ()) {
		d << "_" << dm.facility;
	}

	if (_interop) {
		d << "_IOP";
	} else {
		d << "_SMPTE";
	}
	
	if (three_d ()) {
		d << "-3D";
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
	if (use_isdcf_name()) {
		return isdcf_name (if_created_now);
	}

	return name();
}

void
Film::set_directory (boost::filesystem::path d)
{
	_directory = d;
	_dirty = true;
}

void
Film::set_name (string n)
{
	_name = n;
	signal_changed (NAME);
}

void
Film::set_use_isdcf_name (bool u)
{
	_use_isdcf_name = u;
	signal_changed (USE_ISDCF_NAME);
}

void
Film::set_dcp_content_type (DCPContentType const * t)
{
	_dcp_content_type = t;
	signal_changed (DCP_CONTENT_TYPE);
}

void
Film::set_container (Ratio const * c)
{
	_container = c;
	signal_changed (CONTAINER);
}

void
Film::set_resolution (Resolution r)
{
	_resolution = r;
	signal_changed (RESOLUTION);
}

void
Film::set_scaler (Scaler const * s)
{
	_scaler = s;
	signal_changed (SCALER);
}

void
Film::set_j2k_bandwidth (int b)
{
	_j2k_bandwidth = b;
	signal_changed (J2K_BANDWIDTH);
}

void
Film::set_isdcf_metadata (ISDCFMetadata m)
{
	_isdcf_metadata = m;
	signal_changed (ISDCF_METADATA);
}

void
Film::set_video_frame_rate (int f)
{
	_video_frame_rate = f;
	signal_changed (VIDEO_FRAME_RATE);
}

void
Film::set_audio_channels (int c)
{
	_audio_channels = c;
	signal_changed (AUDIO_CHANNELS);
}

void
Film::set_three_d (bool t)
{
	_three_d = t;
	signal_changed (THREE_D);
}

void
Film::set_interop (bool i)
{
	_interop = i;
	signal_changed (INTEROP);
}

void
Film::set_burn_subtitles (bool b)
{
	_burn_subtitles = b;
	signal_changed (BURN_SUBTITLES);
}

void
Film::signal_changed (Property p)
{
	_dirty = true;

	switch (p) {
	case Film::CONTENT:
		set_video_frame_rate (_playlist->best_dcp_frame_rate ());
		break;
	case Film::VIDEO_FRAME_RATE:
	case Film::SEQUENCE_VIDEO:
		_playlist->maybe_sequence_video ();
		break;
	default:
		break;
	}

	if (ui_signaller) {
		ui_signaller->emit (boost::bind (boost::ref (Changed), p));
	}
}

void
Film::set_isdcf_date_today ()
{
	_isdcf_date = boost::gregorian::day_clock::local_day ();
}

boost::filesystem::path
Film::info_path (int f, Eyes e) const
{
	boost::filesystem::path p;
	p /= info_dir ();

	SafeStringStream s;
	s.width (8);
	s << setfill('0') << f;

	if (e == EYES_LEFT) {
		s << ".L";
	} else if (e == EYES_RIGHT) {
		s << ".R";
	}

	s << ".md5";
	
	p /= s.str();

	/* info_dir() will already have added any initial bit of the path,
	   so don't call file() on this.
	*/
	return p;
}

boost::filesystem::path
Film::j2c_path (int f, Eyes e, bool t) const
{
	boost::filesystem::path p;
	p /= "j2c";
	p /= video_identifier ();

	SafeStringStream s;
	s.width (8);
	s << setfill('0') << f;

	if (e == EYES_LEFT) {
		s << ".L";
	} else if (e == EYES_RIGHT) {
		s << ".R";
	}
	
	s << ".j2c";

	if (t) {
		s << ".tmp";
	}

	p /= s.str();
	return file (p);
}

/** Find all the DCPs in our directory that can be dcp::DCP::read() and return details of their CPLs */
vector<CPLSummary>
Film::cpls () const
{
	vector<CPLSummary> out;
	
	boost::filesystem::path const dir = directory ();
	for (boost::filesystem::directory_iterator i = boost::filesystem::directory_iterator(dir); i != boost::filesystem::directory_iterator(); ++i) {
		if (
			boost::filesystem::is_directory (*i) &&
			i->path().leaf() != "j2c" && i->path().leaf() != "video" && i->path().leaf() != "info" && i->path().leaf() != "analysis"
			) {

			try {
				dcp::DCP dcp (*i);
				dcp.read ();
				out.push_back (
					CPLSummary (
						i->path().leaf().string(),
						dcp.cpls().front()->id(),
						dcp.cpls().front()->annotation_text(),
						dcp.cpls().front()->file()
						)
					);
			} catch (...) {

			}
		}
	}
	
	return out;
}

shared_ptr<Player>
Film::make_player () const
{
	return shared_ptr<Player> (new Player (shared_from_this (), _playlist));
}

void
Film::set_signed (bool s)
{
	_signed = s;
	signal_changed (SIGNED);
}

void
Film::set_encrypted (bool e)
{
	_encrypted = e;
	signal_changed (ENCRYPTED);
}

shared_ptr<Playlist>
Film::playlist () const
{
	return _playlist;
}

ContentList
Film::content () const
{
	return _playlist->content ();
}

void
Film::examine_content (shared_ptr<Content> c, bool calculate_digest)
{
	shared_ptr<Job> j (new ExamineContentJob (shared_from_this(), c, calculate_digest));
	JobManager::instance()->add (j);
}

void
Film::examine_and_add_content (shared_ptr<Content> c, bool calculate_digest)
{
	if (dynamic_pointer_cast<FFmpegContent> (c)) {
		run_ffprobe (c->path(0), file ("ffprobe.log"), _log);
	}
			
	shared_ptr<Job> j (new ExamineContentJob (shared_from_this(), c, calculate_digest));
	j->Finished.connect (bind (&Film::maybe_add_content, this, boost::weak_ptr<Job> (j), boost::weak_ptr<Content> (c)));
	JobManager::instance()->add (j);
}

void
Film::maybe_add_content (weak_ptr<Job> j, weak_ptr<Content> c)
{
	shared_ptr<Job> job = j.lock ();
	if (!job || !job->finished_ok ()) {
		return;
	}
	
	shared_ptr<Content> content = c.lock ();
	if (content) {
		add_content (content);
	}
}

void
Film::add_content (shared_ptr<Content> c)
{
	/* Add video content after any existing content */
	if (dynamic_pointer_cast<VideoContent> (c)) {
		c->set_position (_playlist->video_end ());
	}

	_playlist->add (c);
}

void
Film::remove_content (shared_ptr<Content> c)
{
	_playlist->remove (c);
}

void
Film::move_content_earlier (shared_ptr<Content> c)
{
	_playlist->move_earlier (c);
}

void
Film::move_content_later (shared_ptr<Content> c)
{
	_playlist->move_later (c);
}

DCPTime
Film::length () const
{
	return _playlist->length ();
}

int
Film::best_video_frame_rate () const
{
	return _playlist->best_dcp_frame_rate ();
}

FrameRateChange
Film::active_frame_rate_change (DCPTime t) const
{
	return _playlist->active_frame_rate_change (t, video_frame_rate ());
}

void
Film::playlist_content_changed (boost::weak_ptr<Content> c, int p)
{
	if (p == VideoContentProperty::VIDEO_FRAME_RATE) {
		set_video_frame_rate (_playlist->best_dcp_frame_rate ());
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
Film::audio_frame_rate () const
{
	/* XXX */
	return 48000;
}

void
Film::set_sequence_video (bool s)
{
	_sequence_video = s;
	_playlist->set_sequence_video (s);
	signal_changed (SEQUENCE_VIDEO);
}

/** @return Size of the largest possible image in whatever resolution we are using */
dcp::Size
Film::full_frame () const
{
	switch (_resolution) {
	case RESOLUTION_2K:
		return dcp::Size (2048, 1080);
	case RESOLUTION_4K:
		return dcp::Size (4096, 2160);
	}

	DCPOMATIC_ASSERT (false);
	return dcp::Size ();
}

/** @return Size of the frame */
dcp::Size
Film::frame_size () const
{
	return fit_ratio_within (container()->ratio(), full_frame (), 1);
}

dcp::EncryptedKDM
Film::make_kdm (
	dcp::Certificate target,
	boost::filesystem::path cpl_file,
	dcp::LocalTime from,
	dcp::LocalTime until,
	dcp::Formulation formulation
	) const
{
	shared_ptr<const dcp::CPL> cpl (new dcp::CPL (cpl_file));
	shared_ptr<const dcp::Signer> signer = Config::instance()->signer();
	if (!signer->valid ()) {
		throw InvalidSignerError ();
	}
	
	return dcp::DecryptedKDM (
		cpl, key(), from, until, "DCP-o-matic", cpl->content_title_text(), dcp::LocalTime().as_string()
		).encrypt (signer, target, formulation);
}

list<dcp::EncryptedKDM>
Film::make_kdms (
	list<shared_ptr<Screen> > screens,
	boost::filesystem::path dcp,
	dcp::LocalTime from,
	dcp::LocalTime until,
	dcp::Formulation formulation
	) const
{
	list<dcp::EncryptedKDM> kdms;

	for (list<shared_ptr<Screen> >::iterator i = screens.begin(); i != screens.end(); ++i) {
		if ((*i)->certificate) {
			kdms.push_back (make_kdm ((*i)->certificate.get(), dcp, from, until, formulation));
		}
	}

	return kdms;
}

/** @return The approximate disk space required to encode a DCP of this film with the
 *  current settings, in bytes.
 */
uint64_t
Film::required_disk_space () const
{
	return uint64_t (j2k_bandwidth() / 8) * length().seconds();
}

/** This method checks the disk that the Film is on and tries to decide whether or not
 *  there will be enough space to make a DCP for it.  If so, true is returned; if not,
 *  false is returned and required and availabe are filled in with the amount of disk space
 *  required and available respectively (in Gb).
 *
 *  Note: the decision made by this method isn't, of course, 100% reliable.
 */
bool
Film::should_be_enough_disk_space (double& required, double& available) const
{
	boost::filesystem::space_info s = boost::filesystem::space (internal_video_mxf_dir ());
	required = double (required_disk_space ()) / 1073741824.0f;
	available = double (s.available) / 1073741824.0f;
	return (available - required) > 1;
}

string
Film::subtitle_language () const
{
	set<string> languages;
	
	ContentList cl = content ();
	BOOST_FOREACH (shared_ptr<Content>& c, cl) {
		shared_ptr<SubtitleContent> sc = dynamic_pointer_cast<SubtitleContent> (c);
		if (sc) {
			languages.insert (sc->subtitle_language ());
		}
	}

	string all;
	BOOST_FOREACH (string s, languages) {
		if (!all.empty ()) {
			all += "/" + s;
		} else {
			all += s;
		}
	}

	return all;
}
