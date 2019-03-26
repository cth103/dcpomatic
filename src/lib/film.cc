/*
    Copyright (C) 2012-2019 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/

/** @file  src/film.cc
 *  @brief A representation of some audio, video and subtitle content, and details of
 *  how they should be presented in a DCP.
 */

#include "film.h"
#include "job.h"
#include "util.h"
#include "job_manager.h"
#include "dcp_encoder.h"
#include "transcode_job.h"
#include "upload_job.h"
#include "null_log.h"
#include "file_log.h"
#include "dcpomatic_log.h"
#include "exceptions.h"
#include "examine_content_job.h"
#include "config.h"
#include "playlist.h"
#include "dcp_content_type.h"
#include "ratio.h"
#include "cross.h"
#include "environment_info.h"
#include "audio_processor.h"
#include "digester.h"
#include "compose.hpp"
#include "screen.h"
#include "audio_content.h"
#include "video_content.h"
#include "text_content.h"
#include "ffmpeg_content.h"
#include "dcp_content.h"
#include "screen_kdm.h"
#include "cinema.h"
#include "change_signaller.h"
#include "check_content_change_job.h"
#include <libcxml/cxml.h>
#include <dcp/cpl.h>
#include <dcp/certificate_chain.h>
#include <dcp/util.h>
#include <dcp/local_time.h>
#include <dcp/decrypted_kdm.h>
#include <dcp/raw_convert.h>
#include <dcp/reel_mxf.h>
#include <dcp/reel_asset.h>
#include <libxml++/libxml++.h>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>
#include <unistd.h>
#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <set>

#include "i18n.h"

using std::string;
using std::pair;
using std::vector;
using std::setfill;
using std::min;
using std::max;
using std::make_pair;
using std::cout;
using std::list;
using std::set;
using std::runtime_error;
using std::copy;
using std::back_inserter;
using std::map;
using std::exception;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;
using boost::optional;
using boost::is_any_of;
using dcp::raw_convert;

string const Film::metadata_file = "metadata.xml";

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
 *
 * 32 -> 33
 * Changed <Period> to <Subtitle> in FFmpegSubtitleStream
 * 33 -> 34
 * Content only contains audio/subtitle-related tags if those things
 * are present.
 * 34 -> 35
 * VideoFrameType in VideoContent is a string rather than an integer.
 * 35 -> 36
 * EffectColour rather than OutlineColour in Subtitle.
 * 36 -> 37
 * TextContent can be in a Caption tag, and some of the tag names
 * have had Subtitle prefixes or suffixes removed.
 */
int const Film::current_state_version = 37;

/** Construct a Film object in a given directory.
 *
 *  @param dir Film directory.
 */

Film::Film (optional<boost::filesystem::path> dir)
	: _playlist (new Playlist)
	, _use_isdcf_name (true)
	, _dcp_content_type (Config::instance()->default_dcp_content_type ())
	, _container (Config::instance()->default_container ())
	, _resolution (RESOLUTION_2K)
	, _signed (true)
	, _encrypted (false)
	, _context_id (dcp::make_uuid ())
	, _j2k_bandwidth (Config::instance()->default_j2k_bandwidth ())
	, _isdcf_metadata (Config::instance()->default_isdcf_metadata ())
	, _video_frame_rate (24)
	, _audio_channels (Config::instance()->default_dcp_audio_channels ())
	, _three_d (false)
	, _sequence (true)
	, _interop (Config::instance()->default_interop ())
	, _audio_processor (0)
	, _reel_type (REELTYPE_SINGLE)
	, _reel_length (2000000000)
	, _upload_after_make_dcp (Config::instance()->default_upload_after_make_dcp())
	, _reencode_j2k (false)
	, _user_explicit_video_frame_rate (false)
	, _state_version (current_state_version)
	, _dirty (false)
{
	set_isdcf_date_today ();

	_playlist_change_connection = _playlist->Change.connect (bind (&Film::playlist_change, this, _1));
	_playlist_order_changed_connection = _playlist->OrderChanged.connect (bind (&Film::playlist_order_changed, this));
	_playlist_content_change_connection = _playlist->ContentChange.connect (bind (&Film::playlist_content_change, this, _1, _2, _3, _4));

	if (dir) {
		/* Make state.directory a complete path without ..s (where possible)
		   (Code swiped from Adam Bowen on stackoverflow)
		   XXX: couldn't/shouldn't this just be boost::filesystem::canonical?
		*/

		boost::filesystem::path p (boost::filesystem::system_complete (dir.get()));
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
	}

	if (_directory) {
		_log.reset (new FileLog (file ("log")));
	} else {
		_log.reset (new NullLog);
	}

	_playlist->set_sequence (_sequence);
}

Film::~Film ()
{
	BOOST_FOREACH (boost::signals2::connection& i, _job_connections) {
		i.disconnect ();
	}

	BOOST_FOREACH (boost::signals2::connection& i, _audio_analysis_connections) {
		i.disconnect ();
	}
}

string
Film::video_identifier () const
{
	DCPOMATIC_ASSERT (container ());

	string s = container()->id()
		+ "_" + resolution_to_string (_resolution)
		+ "_" + _playlist->video_identifier()
		+ "_" + raw_convert<string>(_video_frame_rate)
		+ "_" + raw_convert<string>(j2k_bandwidth());

	if (encrypted ()) {
		s += "_E";
	} else {
		s += "_P";
	}

	if (_interop) {
		s += "_I";
	} else {
		s += "_S";
	}

	if (_three_d) {
		s += "_3D";
	}

	return s;
}

/** @return The file to write video frame info to */
boost::filesystem::path
Film::info_file (DCPTimePeriod period) const
{
	boost::filesystem::path p;
	p /= "info";
	p /= video_identifier () + "_" + raw_convert<string> (period.from.get()) + "_" + raw_convert<string> (period.to.get());
	return file (p);
}

boost::filesystem::path
Film::internal_video_asset_dir () const
{
	return dir ("video");
}

boost::filesystem::path
Film::internal_video_asset_filename (DCPTimePeriod p) const
{
	return video_identifier() + "_" + raw_convert<string> (p.from.get()) + "_" + raw_convert<string> (p.to.get()) + ".mxf";
}

boost::filesystem::path
Film::audio_analysis_path (shared_ptr<const Playlist> playlist) const
{
	boost::filesystem::path p = dir ("analysis");

	Digester digester;
	BOOST_FOREACH (shared_ptr<Content> i, playlist->content ()) {
		if (!i->audio) {
			continue;
		}

		digester.add (i->digest ());
		digester.add (i->audio->mapping().digest ());
		if (playlist->content().size() != 1) {
			/* Analyses should be considered equal regardless of gain
			   if they were made from just one piece of content.  This
			   is because we can fake any gain change in a single-content
			   analysis at the plotting stage rather than having to
			   recompute it.
			*/
			digester.add (i->audio->gain ());
		}
	}

	if (audio_processor ()) {
		digester.add (audio_processor()->id ());
	}

	digester.add (audio_channels());

	p /= digester.get ();
	return p;
}

/** Add suitable Jobs to the JobManager to create a DCP for this Film */
void
Film::make_dcp ()
{
	if (dcp_name().find ("/") != string::npos) {
		throw BadSettingError (_("name"), _("Cannot contain slashes"));
	}

	if (container() == 0) {
		throw MissingSettingError (_("container"));
	}

	if (content().empty()) {
		throw runtime_error (_("You must add some content to the DCP before creating it"));
	}

	if (dcp_content_type() == 0) {
		throw MissingSettingError (_("content type"));
	}

	if (name().empty()) {
		set_name ("DCP");
	}

	BOOST_FOREACH (shared_ptr<const Content> i, content ()) {
		if (!i->paths_valid()) {
			throw runtime_error (_("some of your content is missing"));
		}
		shared_ptr<const DCPContent> dcp = dynamic_pointer_cast<const DCPContent> (i);
		if (dcp && dcp->needs_kdm()) {
			throw runtime_error (_("Some of your content needs a KDM"));
		}
		if (dcp && dcp->needs_assets()) {
			throw runtime_error (_("Some of your content needs an OV"));
		}
	}

	set_isdcf_date_today ();

	BOOST_FOREACH (string i, environment_info ()) {
		LOG_GENERAL_NC (i);
	}

	BOOST_FOREACH (shared_ptr<const Content> i, content ()) {
		LOG_GENERAL ("Content: %1", i->technical_summary());
	}
	LOG_GENERAL ("DCP video rate %1 fps", video_frame_rate());
	if (Config::instance()->only_servers_encode ()) {
		LOG_GENERAL_NC ("0 threads: ONLY SERVERS SET TO ENCODE");
	} else {
		LOG_GENERAL ("%1 threads", Config::instance()->master_encoding_threads());
	}
	LOG_GENERAL ("J2K bandwidth %1", j2k_bandwidth());

	shared_ptr<TranscodeJob> tj (new TranscodeJob (shared_from_this()));
	tj->set_encoder (shared_ptr<Encoder> (new DCPEncoder (shared_from_this(), tj)));
	shared_ptr<CheckContentChangeJob> cc (new CheckContentChangeJob (shared_from_this(), tj));
	JobManager::instance()->add (cc);
}

/** Start a job to send our DCP to the configured TMS */
void
Film::send_dcp_to_tms ()
{
	shared_ptr<Job> j (new UploadJob (shared_from_this()));
	JobManager::instance()->add (j);
}

shared_ptr<xmlpp::Document>
Film::metadata (bool with_content_paths) const
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
	root->add_child("J2KBandwidth")->add_child_text (raw_convert<string> (_j2k_bandwidth));
	_isdcf_metadata.as_xml (root->add_child ("ISDCFMetadata"));
	root->add_child("VideoFrameRate")->add_child_text (raw_convert<string> (_video_frame_rate));
	root->add_child("ISDCFDate")->add_child_text (boost::gregorian::to_iso_string (_isdcf_date));
	root->add_child("AudioChannels")->add_child_text (raw_convert<string> (_audio_channels));
	root->add_child("ThreeD")->add_child_text (_three_d ? "1" : "0");
	root->add_child("Sequence")->add_child_text (_sequence ? "1" : "0");
	root->add_child("Interop")->add_child_text (_interop ? "1" : "0");
	root->add_child("Signed")->add_child_text (_signed ? "1" : "0");
	root->add_child("Encrypted")->add_child_text (_encrypted ? "1" : "0");
	root->add_child("Key")->add_child_text (_key.hex ());
	root->add_child("ContextID")->add_child_text (_context_id);
	if (_audio_processor) {
		root->add_child("AudioProcessor")->add_child_text (_audio_processor->id ());
	}
	root->add_child("ReelType")->add_child_text (raw_convert<string> (static_cast<int> (_reel_type)));
	root->add_child("ReelLength")->add_child_text (raw_convert<string> (_reel_length));
	root->add_child("UploadAfterMakeDCP")->add_child_text (_upload_after_make_dcp ? "1" : "0");
	root->add_child("ReencodeJ2K")->add_child_text (_reencode_j2k ? "1" : "0");
	root->add_child("UserExplicitVideoFrameRate")->add_child_text(_user_explicit_video_frame_rate ? "1" : "0");
	for (map<dcp::Marker, DCPTime>::const_iterator i = _markers.begin(); i != _markers.end(); ++i) {
		xmlpp::Element* m = root->add_child("Marker");
		m->set_attribute("Type", dcp::marker_to_string(i->first));
		m->add_child_text(raw_convert<string>(i->second.get()));
	}
	BOOST_FOREACH (dcp::Rating i, _ratings) {
		i.as_xml (root->add_child("Rating"));
	}
	_playlist->as_xml (root->add_child ("Playlist"), with_content_paths);

	return doc;
}

void
Film::write_metadata (boost::filesystem::path path) const
{
	shared_ptr<xmlpp::Document> doc = metadata ();
	doc->write_to_file_formatted (path.string());
}

/** Write state to our `metadata' file */
void
Film::write_metadata () const
{
	DCPOMATIC_ASSERT (directory());
	boost::filesystem::create_directories (directory().get());
	shared_ptr<xmlpp::Document> doc = metadata ();
	doc->write_to_file_formatted (file(metadata_file).string ());
	_dirty = false;
}

/** Write a template from this film */
void
Film::write_template (boost::filesystem::path path) const
{
	boost::filesystem::create_directories (path.parent_path());
	shared_ptr<xmlpp::Document> doc = metadata (false);
	doc->write_to_file_formatted (path.string ());
}

/** Read state from our metadata file.
 *  @return Notes about things that the user should know about, or empty.
 */
list<string>
Film::read_metadata (optional<boost::filesystem::path> path)
{
	if (!path) {
		if (boost::filesystem::exists (file ("metadata")) && !boost::filesystem::exists (file (metadata_file))) {
			throw runtime_error (_("This film was created with an older version of DCP-o-matic, and unfortunately it cannot be loaded into this version.  You will need to create a new Film, re-add your content and set it up again.  Sorry!"));
		}

		path = file (metadata_file);
	}

	cxml::Document f ("Metadata");
	f.read_file (path.get ());

	_state_version = f.number_child<int> ("Version");
	if (_state_version > current_state_version) {
		throw runtime_error (_("This film was created with a newer version of DCP-o-matic, and it cannot be loaded into this version.  Sorry!"));
	} else if (_state_version < current_state_version) {
		/* This is an older version; save a copy (if we haven't already) */
		boost::filesystem::path const older = path->parent_path() / String::compose("metadata.%1.xml", _state_version);
		if (!boost::filesystem::is_regular_file(older)) {
			try {
				boost::filesystem::copy_file(*path, older);
			} catch (...) {
				/* Never mind; at least we tried */
			}
		}
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
	_j2k_bandwidth = f.number_child<int> ("J2KBandwidth");
	_video_frame_rate = f.number_child<int> ("VideoFrameRate");
	_signed = f.optional_bool_child("Signed").get_value_or (true);
	_encrypted = f.bool_child ("Encrypted");
	_audio_channels = f.number_child<int> ("AudioChannels");
	/* We used to allow odd numbers (and zero) channels, but it's just not worth
	   the pain.
	*/
	if (_audio_channels == 0) {
		_audio_channels = 2;
	} else if ((_audio_channels % 2) == 1) {
		_audio_channels++;
	}

	if (f.optional_bool_child("SequenceVideo")) {
		_sequence = f.bool_child("SequenceVideo");
	} else {
		_sequence = f.bool_child("Sequence");
	}

	_three_d = f.bool_child ("ThreeD");
	_interop = f.bool_child ("Interop");
	_key = dcp::Key (f.string_child ("Key"));
	_context_id = f.optional_string_child("ContextID").get_value_or (dcp::make_uuid ());

	if (f.optional_string_child ("AudioProcessor")) {
		_audio_processor = AudioProcessor::from_id (f.string_child ("AudioProcessor"));
	} else {
		_audio_processor = 0;
	}

	_reel_type = static_cast<ReelType> (f.optional_number_child<int>("ReelType").get_value_or (static_cast<int>(REELTYPE_SINGLE)));
	_reel_length = f.optional_number_child<int64_t>("ReelLength").get_value_or (2000000000);
	_upload_after_make_dcp = f.optional_bool_child("UploadAfterMakeDCP").get_value_or (false);
	_reencode_j2k = f.optional_bool_child("ReencodeJ2K").get_value_or(false);
	_user_explicit_video_frame_rate = f.optional_bool_child("UserExplicitVideoFrameRate").get_value_or(false);

	BOOST_FOREACH (cxml::ConstNodePtr i, f.node_children("Marker")) {
		_markers[dcp::marker_from_string(i->string_attribute("Type"))] = DCPTime(dcp::raw_convert<DCPTime::Type>(i->content()));
	}

	BOOST_FOREACH (cxml::ConstNodePtr i, f.node_children("Rating")) {
		_ratings.push_back (dcp::Rating(i));
	}

	list<string> notes;
	/* This method is the only one that can return notes (so far) */
	_playlist->set_from_xml (shared_from_this(), f.node_child ("Playlist"), _state_version, notes);

	/* Write backtraces to this film's directory, until another film is loaded */
	if (_directory) {
		set_backtrace_file (file ("backtrace.txt"));
	}

	_dirty = false;
	return notes;
}

/** Given a directory name, return its full path within the Film's directory.
 *  @param d directory name within the Film's directory.
 *  @param create true to create the directory (and its parents) if they do not exist.
 */
boost::filesystem::path
Film::dir (boost::filesystem::path d, bool create) const
{
	DCPOMATIC_ASSERT (_directory);

	boost::filesystem::path p;
	p /= _directory.get();
	p /= d;

	if (create) {
		boost::filesystem::create_directories (p);
	}

	return p;
}

/** Given a file or directory name, return its full path within the Film's directory.
 *  Any required parent directories will be created.
 */
boost::filesystem::path
Film::file (boost::filesystem::path f) const
{
	DCPOMATIC_ASSERT (_directory);

	boost::filesystem::path p;
	p /= _directory.get();
	p /= f;

	boost::filesystem::create_directories (p.parent_path ());

	return p;
}

list<int>
Film::mapped_audio_channels () const
{
	list<int> mapped;

	if (audio_processor ()) {
		/* Processors are mapped 1:1 to DCP outputs so we can work out mappings from there */
		for (int i = 0; i < audio_processor()->out_channels(); ++i) {
			mapped.push_back (i);
		}
	} else {
		BOOST_FOREACH (shared_ptr<Content> i, content ()) {
			if (i->audio) {
				list<int> c = i->audio->mapping().mapped_output_channels ();
				copy (c.begin(), c.end(), back_inserter (mapped));
			}
		}

		mapped.sort ();
		mapped.unique ();
	}

	return mapped;
}

/** @return a ISDCF-compliant name for a DCP of this film */
string
Film::isdcf_name (bool if_created_now) const
{
	string d;

	string raw_name = name ();

	/* Split the raw name up into words */
	vector<string> words;
	split (words, raw_name, is_any_of (" _-"));

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

	d += fixed_name;

	if (dcp_content_type()) {
		d += "_" + dcp_content_type()->isdcf_name();
		d += "-" + raw_convert<string>(isdcf_metadata().content_version);
	}

	ISDCFMetadata const dm = isdcf_metadata ();

	if (dm.temp_version) {
		d += "-Temp";
	}

	if (dm.pre_release) {
		d += "-Pre";
	}

	if (dm.red_band) {
		d += "-RedBand";
	}

	if (!dm.chain.empty ()) {
		d += "-" + dm.chain;
	}

	if (three_d ()) {
		d += "-3D";
	}

	if (dm.two_d_version_of_three_d) {
		d += "-2D";
	}

	if (!dm.mastered_luminance.empty ()) {
		d += "-" + dm.mastered_luminance;
	}

	if (video_frame_rate() != 24) {
		d += "-" + raw_convert<string>(video_frame_rate());
	}

	if (container()) {
		d += "_" + container()->isdcf_name();
	}

	/* XXX: this uses the first bit of content only */

	/* Interior aspect ratio.  The standard says we don't do this for trailers, for some strange reason */
	if (dcp_content_type() && dcp_content_type()->libdcp_kind() != dcp::TRAILER) {
		Ratio const * content_ratio = 0;
		BOOST_FOREACH (shared_ptr<Content> i, content ()) {
			if (i->video) {
				/* Here's the first piece of video content */
				if (i->video->scale().ratio ()) {
					content_ratio = i->video->scale().ratio ();
				} else {
					content_ratio = Ratio::from_ratio (i->video->size().ratio ());
				}
				break;
			}
		}

		if (content_ratio && content_ratio != container()) {
			/* This needs to be the numeric version of the ratio, and ::id() is close enough */
			d += "-" + content_ratio->id();
		}
	}

	if (!dm.audio_language.empty ()) {
		d += "_" + dm.audio_language;
		if (!dm.subtitle_language.empty()) {

			/* I'm not clear on the precise details of the convention for CCAP labelling;
			   for now I'm just appending -CCAP if we have any closed captions.
			*/

			bool burnt_in = true;
			bool ccap = false;
			BOOST_FOREACH (shared_ptr<Content> i, content()) {
				BOOST_FOREACH (shared_ptr<TextContent> j, i->text) {
					if (j->type() == TEXT_OPEN_SUBTITLE && j->use() && !j->burn()) {
						burnt_in = false;
					} else if (j->type() == TEXT_CLOSED_CAPTION) {
						ccap = true;
					}
				}
			}

			string language = dm.subtitle_language;
			if (burnt_in && language != "XX") {
				transform (language.begin(), language.end(), language.begin(), ::tolower);
			} else {
				transform (language.begin(), language.end(), language.begin(), ::toupper);
			}

			d += "-" + language;
			if (ccap) {
				d += "-CCAP";
			}
		} else {
			d += "-XX";
		}
	}

	if (!dm.territory.empty ()) {
		d += "_" + dm.territory;
		if (dm.rating.empty ()) {
			d += "-NR";
		} else {
			d += "-" + dm.rating;
		}
	}

	/* Count mapped audio channels */

	pair<int, int> ch = audio_channel_types (mapped_audio_channels(), audio_channels());
	if (!ch.first && !ch.second) {
		d += "_MOS";
	} else if (ch.first) {
		d += String::compose("_%1%2", ch.first, ch.second);
	}

	/* XXX: HI/VI */

	d += "_" + resolution_to_string (_resolution);

	if (!dm.studio.empty ()) {
		d += "_" + dm.studio;
	}

	if (if_created_now) {
		d += "_" + boost::gregorian::to_iso_string (boost::gregorian::day_clock::local_day ());
	} else {
		d += "_" + boost::gregorian::to_iso_string (_isdcf_date);
	}

	if (!dm.facility.empty ()) {
		d += "_" + dm.facility;
	}

	if (_interop) {
		d += "_IOP";
	} else {
		d += "_SMPTE";
	}

	if (three_d ()) {
		d += "-3D";
	}

	bool vf = false;
	BOOST_FOREACH (shared_ptr<Content> i, content ()) {
		shared_ptr<const DCPContent> dc = dynamic_pointer_cast<const DCPContent> (i);
		if (!dc) {
			continue;
		}

		bool any_text = false;
		for (int i = 0; i < TEXT_COUNT; ++i) {
			if (dc->reference_text(static_cast<TextType>(i))) {
				any_text = true;
			}
		}
		if (dc->reference_video() || dc->reference_audio() || any_text) {
			vf = true;
		}
	}

	if (vf) {
		d += "_VF";
	} else {
		d += "_OV";
	}

	return d;
}

/** @return name to give the DCP */
string
Film::dcp_name (bool if_created_now) const
{
	string unfiltered;
	if (use_isdcf_name()) {
		return careful_string_filter (isdcf_name (if_created_now));
	}

	return careful_string_filter (name ());
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
	ChangeSignaller<Film> ch (this, NAME);
	_name = n;
}

void
Film::set_use_isdcf_name (bool u)
{
	ChangeSignaller<Film> ch (this, USE_ISDCF_NAME);
	_use_isdcf_name = u;
}

void
Film::set_dcp_content_type (DCPContentType const * t)
{
	ChangeSignaller<Film> ch (this, DCP_CONTENT_TYPE);
	_dcp_content_type = t;
}

void
Film::set_container (Ratio const * c)
{
	ChangeSignaller<Film> ch (this, CONTAINER);
	_container = c;
}

void
Film::set_resolution (Resolution r)
{
	ChangeSignaller<Film> ch (this, RESOLUTION);
	_resolution = r;
}

void
Film::set_j2k_bandwidth (int b)
{
	ChangeSignaller<Film> ch (this, J2K_BANDWIDTH);
	_j2k_bandwidth = b;
}

void
Film::set_isdcf_metadata (ISDCFMetadata m)
{
	ChangeSignaller<Film> ch (this, ISDCF_METADATA);
	_isdcf_metadata = m;
}

/** @param f New frame rate.
 *  @param user_explicit true if this comes from a direct user instruction, false if it is from
 *  DCP-o-matic being helpful.
 */
void
Film::set_video_frame_rate (int f, bool user_explicit)
{
	ChangeSignaller<Film> ch (this, VIDEO_FRAME_RATE);
	_video_frame_rate = f;
	if (user_explicit) {
		_user_explicit_video_frame_rate = true;
	}
}

void
Film::set_audio_channels (int c)
{
	ChangeSignaller<Film> ch (this, AUDIO_CHANNELS);
	_audio_channels = c;
}

void
Film::set_three_d (bool t)
{
	ChangeSignaller<Film> ch (this, THREE_D);
	_three_d = t;

	if (_three_d && _isdcf_metadata.two_d_version_of_three_d) {
		ChangeSignaller<Film> ch (this, ISDCF_METADATA);
		_isdcf_metadata.two_d_version_of_three_d = false;
	}
}

void
Film::set_interop (bool i)
{
	ChangeSignaller<Film> ch (this, INTEROP);
	_interop = i;
}

void
Film::set_audio_processor (AudioProcessor const * processor)
{
	ChangeSignaller<Film> ch1 (this, AUDIO_PROCESSOR);
	ChangeSignaller<Film> ch2 (this, AUDIO_CHANNELS);
	_audio_processor = processor;
}

void
Film::set_reel_type (ReelType t)
{
	ChangeSignaller<Film> ch (this, REEL_TYPE);
	_reel_type = t;
}

/** @param r Desired reel length in bytes */
void
Film::set_reel_length (int64_t r)
{
	ChangeSignaller<Film> ch (this, REEL_LENGTH);
	_reel_length = r;
}

void
Film::set_upload_after_make_dcp (bool u)
{
	ChangeSignaller<Film> ch (this, UPLOAD_AFTER_MAKE_DCP);
	_upload_after_make_dcp = u;
}

void
Film::set_reencode_j2k (bool r)
{
	ChangeSignaller<Film> ch (this, REENCODE_J2K);
	_reencode_j2k = r;
}

void
Film::signal_change (ChangeType type, int p)
{
	signal_change (type, static_cast<Property>(p));
}

void
Film::signal_change (ChangeType type, Property p)
{
	if (type == CHANGE_TYPE_DONE) {
		_dirty = true;

		if (p == Film::CONTENT) {
			if (!_user_explicit_video_frame_rate) {
				set_video_frame_rate (best_video_frame_rate());
			}
		}

		emit (boost::bind (boost::ref (Change), type, p));

		if (p == Film::VIDEO_FRAME_RATE || p == Film::SEQUENCE) {
			/* We want to call Playlist::maybe_sequence but this must happen after the
			   main signal emission (since the butler will see that emission and un-suspend itself).
			*/
			emit (boost::bind(&Playlist::maybe_sequence, _playlist.get(), shared_from_this()));
		}
	} else {
		Change (type, p);
	}
}

void
Film::set_isdcf_date_today ()
{
	_isdcf_date = boost::gregorian::day_clock::local_day ();
}

boost::filesystem::path
Film::j2c_path (int reel, Frame frame, Eyes eyes, bool tmp) const
{
	boost::filesystem::path p;
	p /= "j2c";
	p /= video_identifier ();

	char buffer[256];
	snprintf(buffer, sizeof(buffer), "%08d_%08" PRId64, reel, frame);
	string s (buffer);

	if (eyes == EYES_LEFT) {
		s += ".L";
	} else if (eyes == EYES_RIGHT) {
		s += ".R";
	}

	s += ".j2c";

	if (tmp) {
		s += ".tmp";
	}

	p /= s;
	return file (p);
}

static
bool
cpl_summary_compare (CPLSummary const & a, CPLSummary const & b)
{
	return a.last_write_time > b.last_write_time;
}

/** Find all the DCPs in our directory that can be dcp::DCP::read() and return details of their CPLs.
 *  The list will be returned in reverse order of timestamp (i.e. most recent first).
 */
vector<CPLSummary>
Film::cpls () const
{
	if (!directory ()) {
		return vector<CPLSummary> ();
	}

	vector<CPLSummary> out;

	boost::filesystem::path const dir = directory().get();
	for (boost::filesystem::directory_iterator i = boost::filesystem::directory_iterator(dir); i != boost::filesystem::directory_iterator(); ++i) {
		if (
			boost::filesystem::is_directory (*i) &&
			i->path().leaf() != "j2c" && i->path().leaf() != "video" && i->path().leaf() != "info" && i->path().leaf() != "analysis"
			) {

			try {
				out.push_back (CPLSummary(*i));
			} catch (...) {

			}
		}
	}

	sort (out.begin(), out.end(), cpl_summary_compare);

	return out;
}

void
Film::set_signed (bool s)
{
	ChangeSignaller<Film> ch (this, SIGNED);
	_signed = s;
}

void
Film::set_encrypted (bool e)
{
	ChangeSignaller<Film> ch (this, ENCRYPTED);
	_encrypted = e;
}

void
Film::set_key (dcp::Key key)
{
	ChangeSignaller<Film> ch (this, KEY);
	_key = key;
}

ContentList
Film::content () const
{
	return _playlist->content ();
}

/** @param content Content to add.
 *  @param disable_audio_analysis true to never do automatic audio analysis, even if it is enabled in configuration.
 */
void
Film::examine_and_add_content (shared_ptr<Content> content, bool disable_audio_analysis)
{
	if (dynamic_pointer_cast<FFmpegContent> (content) && _directory) {
		run_ffprobe (content->path(0), file("ffprobe.log"));
	}

	shared_ptr<Job> j (new ExamineContentJob (shared_from_this(), content));

	_job_connections.push_back (
		j->Finished.connect (bind (&Film::maybe_add_content, this, weak_ptr<Job>(j), weak_ptr<Content>(content), disable_audio_analysis))
		);

	JobManager::instance()->add (j);
}

void
Film::maybe_add_content (weak_ptr<Job> j, weak_ptr<Content> c, bool disable_audio_analysis)
{
	shared_ptr<Job> job = j.lock ();
	if (!job || !job->finished_ok ()) {
		return;
	}

	shared_ptr<Content> content = c.lock ();
	if (!content) {
		return;
	}

	add_content (content);

	if (Config::instance()->automatic_audio_analysis() && content->audio && !disable_audio_analysis) {
		shared_ptr<Playlist> playlist (new Playlist);
		playlist->add (shared_from_this(), content);
		boost::signals2::connection c;
		JobManager::instance()->analyse_audio (
			shared_from_this(), playlist, false, c, bind (&Film::audio_analysis_finished, this)
			);
		_audio_analysis_connections.push_back (c);
	}
}

void
Film::add_content (shared_ptr<Content> c)
{
	/* Add {video,subtitle} content after any existing {video,subtitle} content */
	if (c->video) {
		c->set_position (shared_from_this(), _playlist->video_end(shared_from_this()));
	} else if (!c->text.empty()) {
		c->set_position (shared_from_this(), _playlist->text_end(shared_from_this()));
	}

	if (_template_film) {
		/* Take settings from the first piece of content of c's type in _template */
		BOOST_FOREACH (shared_ptr<Content> i, _template_film->content()) {
			c->take_settings_from (i);
		}
	}

	_playlist->add (shared_from_this(), c);
}

void
Film::remove_content (shared_ptr<Content> c)
{
	_playlist->remove (c);
}

void
Film::move_content_earlier (shared_ptr<Content> c)
{
	_playlist->move_earlier (shared_from_this(), c);
}

void
Film::move_content_later (shared_ptr<Content> c)
{
	_playlist->move_later (shared_from_this(), c);
}

/** @return length of the film from time 0 to the last thing on the playlist */
DCPTime
Film::length () const
{
	return _playlist->length(shared_from_this()).ceil(video_frame_rate());
}

int
Film::best_video_frame_rate () const
{
	/* Don't default to anything above 30fps (make the user select that explicitly) */
	int best = _playlist->best_video_frame_rate ();
	if (best > 30) {
		best /= 2;
	}
	return best;
}

FrameRateChange
Film::active_frame_rate_change (DCPTime t) const
{
	return _playlist->active_frame_rate_change (t, video_frame_rate ());
}

void
Film::playlist_content_change (ChangeType type, weak_ptr<Content> c, int p, bool frequent)
{
	if (p == ContentProperty::VIDEO_FRAME_RATE) {
		signal_change (type, Film::CONTENT);
	} else if (p == AudioContentProperty::STREAMS) {
		signal_change (type, Film::NAME);
	}

	if (type == CHANGE_TYPE_DONE) {
		emit (boost::bind (boost::ref (ContentChange), type, c, p, frequent));
	} else {
		ContentChange (type, c, p, frequent);
	}
}

void
Film::playlist_change (ChangeType type)
{
	signal_change (type, CONTENT);
	signal_change (type, NAME);

	if (type == CHANGE_TYPE_DONE) {
		/* Check that this change hasn't made our settings inconsistent */
		bool change_made = false;
		BOOST_FOREACH (shared_ptr<Content> i, content()) {
			shared_ptr<DCPContent> d = dynamic_pointer_cast<DCPContent>(i);
			if (!d) {
				continue;
			}

			string why_not;
			if (d->reference_video() && !d->can_reference_video(shared_from_this(), why_not)) {
				d->set_reference_video(false);
				change_made = true;
			}
			if (d->reference_audio() && !d->can_reference_audio(shared_from_this(), why_not)) {
				d->set_reference_audio(false);
				change_made = true;
			}
		}

		if (change_made) {
			Message (_("DCP-o-matic had to change your settings for referring to DCPs as OV.  Please review those settings to make sure they are what you want."));
		}
	}
}

void
Film::playlist_order_changed ()
{
	/* XXX: missing PENDING */
	signal_change (CHANGE_TYPE_DONE, CONTENT_ORDER);
}

int
Film::audio_frame_rate () const
{
	/* It seems that nobody makes 96kHz DCPs at the moment, so let's avoid them.
	   See #1436.
	*/
	return 48000;
}

void
Film::set_sequence (bool s)
{
	if (s == _sequence) {
		return;
	}

	ChangeSignaller<Film> cc (this, SEQUENCE);
	_sequence = s;
	_playlist->set_sequence (s);
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
	return fit_ratio_within (container()->ratio(), full_frame ());
}

/** @param recipient KDM recipient certificate.
 *  @param trusted_devices Certificate thumbprints of other trusted devices (can be empty).
 *  @param cpl_file CPL filename.
 *  @param from KDM from time expressed as a local time with an offset from UTC.
 *  @param until KDM to time expressed as a local time with an offset from UTC.
 *  @param formulation KDM formulation to use.
 *  @param disable_forensic_marking_picture true to disable forensic marking of picture.
 *  @param disable_forensic_marking_audio if not set, don't disable forensic marking of audio.  If set to 0,
 *  disable all forensic marking; if set above 0, disable forensic marking above that channel.
 */
dcp::EncryptedKDM
Film::make_kdm (
	dcp::Certificate recipient,
	vector<string> trusted_devices,
	boost::filesystem::path cpl_file,
	dcp::LocalTime from,
	dcp::LocalTime until,
	dcp::Formulation formulation,
	bool disable_forensic_marking_picture,
	optional<int> disable_forensic_marking_audio
	) const
{
	if (!_encrypted) {
		throw runtime_error (_("Cannot make a KDM as this project is not encrypted."));
	}

	shared_ptr<const dcp::CPL> cpl (new dcp::CPL (cpl_file));
	shared_ptr<const dcp::CertificateChain> signer = Config::instance()->signer_chain ();
	if (!signer->valid ()) {
		throw InvalidSignerError ();
	}

	/* Find keys that have been added to imported, encrypted DCP content */
	list<dcp::DecryptedKDMKey> imported_keys;
	BOOST_FOREACH (shared_ptr<Content> i, content()) {
		shared_ptr<DCPContent> d = dynamic_pointer_cast<DCPContent> (i);
		if (d && d->kdm()) {
			dcp::DecryptedKDM kdm (d->kdm().get(), Config::instance()->decryption_chain()->key().get());
			list<dcp::DecryptedKDMKey> keys = kdm.keys ();
			copy (keys.begin(), keys.end(), back_inserter (imported_keys));
		}
	}

	map<shared_ptr<const dcp::ReelMXF>, dcp::Key> keys;

	BOOST_FOREACH(shared_ptr<const dcp::ReelMXF> i, cpl->reel_mxfs()) {
		if (!i->key_id()) {
			continue;
		}

		/* Get any imported key for this ID */
		bool done = false;
		BOOST_FOREACH (dcp::DecryptedKDMKey j, imported_keys) {
			if (j.id() == i->key_id().get()) {
				LOG_GENERAL ("Using imported key for %1", i->key_id().get());
				keys[i] = j.key();
				done = true;
			}
		}

		if (!done) {
			/* No imported key; it must be an asset that we encrypted */
			LOG_GENERAL ("Using our own key for %1", i->key_id().get());
			keys[i] = key();
		}
	}

	return dcp::DecryptedKDM (
		cpl->id(), keys, from, until, cpl->content_title_text(), cpl->content_title_text(), dcp::LocalTime().as_string()
		).encrypt (signer, recipient, trusted_devices, formulation, disable_forensic_marking_picture, disable_forensic_marking_audio);
}

/** @param screens Screens to make KDMs for.
 *  @param cpl_file Path to CPL to make KDMs for.
 *  @param from KDM from time expressed as a local time in the time zone of the Screen's Cinema.
 *  @param until KDM to time expressed as a local time in the time zone of the Screen's Cinema.
 *  @param formulation KDM formulation to use.
 *  @param disable_forensic_marking_picture true to disable forensic marking of picture.
 *  @param disable_forensic_marking_audio if not set, don't disable forensic marking of audio.  If set to 0,
 *  disable all forensic marking; if set above 0, disable forensic marking above that channel.
 */
list<ScreenKDM>
Film::make_kdms (
	list<shared_ptr<Screen> > screens,
	boost::filesystem::path cpl_file,
	boost::posix_time::ptime from,
	boost::posix_time::ptime until,
	dcp::Formulation formulation,
	bool disable_forensic_marking_picture,
	optional<int> disable_forensic_marking_audio
	) const
{
	list<ScreenKDM> kdms;

	BOOST_FOREACH (shared_ptr<Screen> i, screens) {
		if (i->recipient) {
			dcp::EncryptedKDM const kdm = make_kdm (
				i->recipient.get(),
				i->trusted_device_thumbprints(),
				cpl_file,
				dcp::LocalTime (from,  i->cinema ? i->cinema->utc_offset_hour() : 0, i->cinema ? i->cinema->utc_offset_minute() : 0),
				dcp::LocalTime (until, i->cinema ? i->cinema->utc_offset_hour() : 0, i->cinema ? i->cinema->utc_offset_minute() : 0),
				formulation,
				disable_forensic_marking_picture,
				disable_forensic_marking_audio
				);

			kdms.push_back (ScreenKDM (i, kdm));
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
	return _playlist->required_disk_space (shared_from_this(), j2k_bandwidth(), audio_channels(), audio_frame_rate());
}

/** This method checks the disk that the Film is on and tries to decide whether or not
 *  there will be enough space to make a DCP for it.  If so, true is returned; if not,
 *  false is returned and required and available are filled in with the amount of disk space
 *  required and available respectively (in GB).
 *
 *  Note: the decision made by this method isn't, of course, 100% reliable.
 */
bool
Film::should_be_enough_disk_space (double& required, double& available, bool& can_hard_link) const
{
	/* Create a test file and see if we can hard-link it */
	boost::filesystem::path test = internal_video_asset_dir() / "test";
	boost::filesystem::path test2 = internal_video_asset_dir() / "test2";
	can_hard_link = true;
	FILE* f = fopen_boost (test, "w");
	if (f) {
		fclose (f);
		boost::system::error_code ec;
		boost::filesystem::create_hard_link (test, test2, ec);
		if (ec) {
			can_hard_link = false;
		}
		boost::filesystem::remove (test);
		boost::filesystem::remove (test2);
	}

	boost::filesystem::space_info s = boost::filesystem::space (internal_video_asset_dir ());
	required = double (required_disk_space ()) / 1073741824.0f;
	if (!can_hard_link) {
		required *= 2;
	}
	available = double (s.available) / 1073741824.0f;
	return (available - required) > 1;
}

string
Film::subtitle_language () const
{
	set<string> languages;

	BOOST_FOREACH (shared_ptr<Content> i, content()) {
		BOOST_FOREACH (shared_ptr<TextContent> j, i->text) {
			languages.insert (j->language ());
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

/** @return The names of the channels that audio contents' outputs are passed into;
 *  this is either the DCP or a AudioProcessor.
 */
vector<string>
Film::audio_output_names () const
{
	if (audio_processor ()) {
		return audio_processor()->input_names ();
	}

	DCPOMATIC_ASSERT (MAX_DCP_AUDIO_CHANNELS == 16);

	vector<string> n;

	for (int i = 0; i < audio_channels(); ++i) {
		n.push_back (short_audio_channel_name (i));
	}

	return n;
}

void
Film::repeat_content (ContentList c, int n)
{
	_playlist->repeat (shared_from_this(), c, n);
}

void
Film::remove_content (ContentList c)
{
	_playlist->remove (c);
}

void
Film::audio_analysis_finished ()
{
	/* XXX */
}

list<DCPTimePeriod>
Film::reels () const
{
	list<DCPTimePeriod> p;
	DCPTime const len = length();

	switch (reel_type ()) {
	case REELTYPE_SINGLE:
		p.push_back (DCPTimePeriod (DCPTime (), len));
		break;
	case REELTYPE_BY_VIDEO_CONTENT:
	{
		optional<DCPTime> last_split;
		shared_ptr<Content> last_video;
		BOOST_FOREACH (shared_ptr<Content> c, content ()) {
			if (c->video) {
				BOOST_FOREACH (DCPTime t, c->reel_split_points(shared_from_this())) {
					if (last_split) {
						p.push_back (DCPTimePeriod (last_split.get(), t));
					}
					last_split = t;
				}
				last_video = c;
			}
		}

		DCPTime video_end = last_video ? last_video->end(shared_from_this()) : DCPTime(0);
		if (last_split) {
			/* Definitely go from the last split to the end of the video content */
			p.push_back (DCPTimePeriod (last_split.get(), video_end));
		}

		if (video_end < len) {
			/* And maybe go after that as well if there is any non-video hanging over the end */
			p.push_back (DCPTimePeriod (video_end, len));
		}
		break;
	}
	case REELTYPE_BY_LENGTH:
	{
		DCPTime current;
		/* Integer-divide reel length by the size of one frame to give the number of frames per reel */
		Frame const reel_in_frames = _reel_length / ((j2k_bandwidth() / video_frame_rate()) / 8);
		while (current < len) {
			DCPTime end = min (len, current + DCPTime::from_frames (reel_in_frames, video_frame_rate ()));
			p.push_back (DCPTimePeriod (current, end));
			current = end;
		}
		break;
	}
	}

	return p;
}

/** @param period A period within the DCP
 *  @return Name of the content which most contributes to the given period.
 */
string
Film::content_summary (DCPTimePeriod period) const
{
	return _playlist->content_summary (shared_from_this(), period);
}

void
Film::use_template (string name)
{
	_template_film.reset (new Film (optional<boost::filesystem::path>()));
	_template_film->read_metadata (Config::instance()->template_path (name));
	_use_isdcf_name = _template_film->_use_isdcf_name;
	_dcp_content_type = _template_film->_dcp_content_type;
	_container = _template_film->_container;
	_resolution = _template_film->_resolution;
	_j2k_bandwidth = _template_film->_j2k_bandwidth;
	_video_frame_rate = _template_film->_video_frame_rate;
	_signed = _template_film->_signed;
	_encrypted = _template_film->_encrypted;
	_audio_channels = _template_film->_audio_channels;
	_sequence = _template_film->_sequence;
	_three_d = _template_film->_three_d;
	_interop = _template_film->_interop;
	_audio_processor = _template_film->_audio_processor;
	_reel_type = _template_film->_reel_type;
	_reel_length = _template_film->_reel_length;
	_upload_after_make_dcp = _template_film->_upload_after_make_dcp;
	_isdcf_metadata = _template_film->_isdcf_metadata;
}

pair<double, double>
Film::speed_up_range (int dcp_frame_rate) const
{
	return _playlist->speed_up_range (dcp_frame_rate);
}

void
Film::copy_from (shared_ptr<const Film> film)
{
	read_metadata (film->file (metadata_file));
}

bool
Film::references_dcp_video () const
{
	BOOST_FOREACH (shared_ptr<Content> i, _playlist->content()) {
		shared_ptr<DCPContent> d = dynamic_pointer_cast<DCPContent>(i);
		if (d && d->reference_video()) {
			return true;
		}
	}

	return false;
}

bool
Film::references_dcp_audio () const
{
	BOOST_FOREACH (shared_ptr<Content> i, _playlist->content()) {
		shared_ptr<DCPContent> d = dynamic_pointer_cast<DCPContent>(i);
		if (d && d->reference_audio()) {
			return true;
		}
	}

	return false;
}

list<DCPTextTrack>
Film::closed_caption_tracks () const
{
	list<DCPTextTrack> tt;
	BOOST_FOREACH (shared_ptr<Content> i, content()) {
		BOOST_FOREACH (shared_ptr<TextContent> j, i->text) {
			/* XXX: Empty DCPTextTrack ends up being a magic value here */
			DCPTextTrack dtt = j->dcp_track().get_value_or(DCPTextTrack());
			if (j->type() == TEXT_CLOSED_CAPTION && find(tt.begin(), tt.end(), dtt) == tt.end()) {
				tt.push_back (dtt);
			}
		}
	}

	return tt;
}

void
Film::set_marker (dcp::Marker type, DCPTime time)
{
	ChangeSignaller<Film> ch (this, MARKERS);
	_markers[type] = time;
}

void
Film::unset_marker (dcp::Marker type)
{
	ChangeSignaller<Film> ch (this, MARKERS);
	_markers.erase (type);
}

void
Film::set_ratings (vector<dcp::Rating> r)
{
	ChangeSignaller<Film> ch (this, RATINGS);
	_ratings = r;
}

optional<DCPTime>
Film::marker (dcp::Marker type) const
{
	map<dcp::Marker, DCPTime>::const_iterator i = _markers.find (type);
	if (i == _markers.end()) {
		return optional<DCPTime>();
	}
	return i->second;
}
