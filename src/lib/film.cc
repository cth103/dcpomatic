/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include "atmos_content.h"
#include "audio_content.h"
#include "audio_processor.h"
#include "change_signaller.h"
#include "cinema.h"
#include "compose.hpp"
#include "config.h"
#include "cross.h"
#include "dcp_content.h"
#include "dcp_content_type.h"
#include "dcp_encoder.h"
#include "dcpomatic_log.h"
#include "digester.h"
#include "environment_info.h"
#include "examine_content_job.h"
#include "exceptions.h"
#include "ffmpeg_content.h"
#include "ffmpeg_subtitle_stream.h"
#include "file_log.h"
#include "film.h"
#include "font.h"
#include "job.h"
#include "job_manager.h"
#include "kdm_with_metadata.h"
#include "null_log.h"
#include "playlist.h"
#include "ratio.h"
#include "screen.h"
#include "text_content.h"
#include "transcode_job.h"
#include "upload_job.h"
#include "util.h"
#include "video_content.h"
#include "version.h"
#include <libcxml/cxml.h>
#include <dcp/certificate_chain.h>
#include <dcp/cpl.h>
#include <dcp/decrypted_kdm.h>
#include <dcp/local_time.h>
#include <dcp/raw_convert.h>
#include <dcp/reel_asset.h>
#include <dcp/reel_file_asset.h>
#include <dcp/util.h>
#include <libxml++/libxml++.h>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <unistd.h>
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <set>
#include <stdexcept>

#include "i18n.h"


using std::back_inserter;
using std::copy;
using std::cout;
using std::dynamic_pointer_cast;
using std::exception;
using std::find;
using std::list;
using std::make_pair;
using std::make_shared;
using std::map;
using std::max;
using std::min;
using std::pair;
using std::runtime_error;
using std::set;
using std::setfill;
using std::shared_ptr;
using std::string;
using std::vector;
using std::weak_ptr;
using boost::optional;
using boost::is_any_of;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using dcp::raw_convert;
using namespace dcpomatic;


static constexpr char metadata_file[] = "metadata.xml";


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
 * 37 -> 38
 * VideoContent scale expressed just as "guess" or "custom"
 */
int const Film::current_state_version = 38;


/** Construct a Film object in a given directory.
 *
 *  @param dir Film directory.
 */

Film::Film (optional<boost::filesystem::path> dir)
	: _playlist (new Playlist)
	, _use_isdcf_name (Config::instance()->use_isdcf_name_by_default())
	, _dcp_content_type (Config::instance()->default_dcp_content_type ())
	, _container (Config::instance()->default_container ())
	, _resolution (Resolution::TWO_K)
	, _encrypted (false)
	, _context_id (dcp::make_uuid ())
	, _j2k_bandwidth (Config::instance()->default_j2k_bandwidth ())
	, _video_frame_rate (24)
	, _audio_channels (Config::instance()->default_dcp_audio_channels ())
	, _three_d (false)
	, _sequence (true)
	, _interop (Config::instance()->default_interop ())
	, _audio_processor (0)
	, _reel_type (ReelType::SINGLE)
	, _reel_length (2000000000)
	, _reencode_j2k (false)
	, _user_explicit_video_frame_rate (false)
	, _user_explicit_container (false)
	, _user_explicit_resolution (false)
	, _name_language (dcp::LanguageTag("en-US"))
	, _version_number (1)
	, _status (dcp::Status::FINAL)
	, _state_version (current_state_version)
	, _dirty (false)
	, _tolerant (false)
{
	set_isdcf_date_today ();

	auto metadata = Config::instance()->default_metadata();
	if (metadata.find("chain") != metadata.end()) {
		_chain = metadata["chain"];
	}
	if (metadata.find("distributor") != metadata.end()) {
		_distributor = metadata["distributor"];
	}
	if (metadata.find("facility") != metadata.end()) {
		_facility = metadata["facility"];
	}
	if (metadata.find("studio") != metadata.end()) {
		_studio = metadata["studio"];
	}

	_playlist_change_connection = _playlist->Change.connect (bind (&Film::playlist_change, this, _1));
	_playlist_order_changed_connection = _playlist->OrderChange.connect (bind (&Film::playlist_order_changed, this));
	_playlist_content_change_connection = _playlist->ContentChange.connect (bind (&Film::playlist_content_change, this, _1, _2, _3, _4));
	_playlist_length_change_connection = _playlist->LengthChange.connect (bind(&Film::playlist_length_change, this));

	if (dir) {
		/* Make state.directory a complete path without ..s (where possible)
		   (Code swiped from Adam Bowen on stackoverflow)
		   XXX: couldn't/shouldn't this just be boost::filesystem::canonical?
		*/

		boost::filesystem::path p (boost::filesystem::system_complete (dir.get()));
		boost::filesystem::path result;
		for (auto i: p) {
			if (i == "..") {
				boost::system::error_code ec;
				if (boost::filesystem::is_symlink(result, ec) || result.filename() == "..") {
					result /= i;
				} else {
					result = result.parent_path ();
				}
			} else if (i != ".") {
				result /= i;
			}
		}

		set_directory (result.make_preferred ());
	}

	if (_directory) {
		_log = make_shared<FileLog>(file("log"));
	} else {
		_log = make_shared<NullLog>();
	}

	_playlist->set_sequence (_sequence);
}

Film::~Film ()
{
	for (auto& i: _job_connections) {
		i.disconnect ();
	}

	for (auto& i: _audio_analysis_connections) {
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
		/* This is insecure but hey, the key is in plaintext in metadata.xml */
		s += "_E" + _key.hex();
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

	if (_reencode_j2k) {
		s += "_R";
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
	auto p = dir ("analysis");

	Digester digester;
	for (auto i: playlist->content()) {
		if (!i->audio) {
			continue;
		}

		digester.add (i->digest());
		digester.add (i->audio->mapping().digest());
		if (playlist->content().size() != 1) {
			/* Analyses should be considered equal regardless of gain
			   if they were made from just one piece of content.  This
			   is because we can fake any gain change in a single-content
			   analysis at the plotting stage rather than having to
			   recompute it.
			*/
			digester.add (i->audio->gain());

			/* Likewise we only care about position if we're looking at a
			 * whole-project view.
			 */
			digester.add (i->position().get());
			digester.add (i->trim_start().get());
			digester.add (i->trim_end().get());
		}
	}

	if (audio_processor ()) {
		digester.add (audio_processor()->id ());
	}

	digester.add (audio_channels());

	p /= digester.get ();
	return p;
}


boost::filesystem::path
Film::subtitle_analysis_path (shared_ptr<const Content> content) const
{
	auto p = dir ("analysis");

	Digester digester;
	digester.add (content->digest());

	if (!content->text.empty()) {
		auto tc = content->text.front();
		digester.add (tc->x_scale());
		digester.add (tc->y_scale());
		for (auto i: tc->fonts()) {
			digester.add (i->id());
		}
		if (tc->effect()) {
			digester.add (tc->effect().get());
		}
		digester.add (tc->line_spacing());
		digester.add (tc->outline_width());
	}

	auto fc = dynamic_pointer_cast<const FFmpegContent>(content);
	if (fc) {
		digester.add (fc->subtitle_stream()->identifier());
	}

	p /= digester.get ();
	return p;
}


/** Start a job to send our DCP to the configured TMS */
void
Film::send_dcp_to_tms ()
{
	JobManager::instance()->add(make_shared<UploadJob>(shared_from_this()));
}

shared_ptr<xmlpp::Document>
Film::metadata (bool with_content_paths) const
{
	auto doc = make_shared<xmlpp::Document>();
	auto root = doc->create_root_node ("Metadata");

	root->add_child("Version")->add_child_text (raw_convert<string> (current_state_version));
	auto last_write = root->add_child("LastWrittenBy");
	last_write->add_child_text (dcpomatic_version);
	last_write->set_attribute("git", dcpomatic_git_commit);
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
	root->add_child("VideoFrameRate")->add_child_text (raw_convert<string> (_video_frame_rate));
	root->add_child("AudioFrameRate")->add_child_text(raw_convert<string>(_audio_frame_rate));
	root->add_child("ISDCFDate")->add_child_text (boost::gregorian::to_iso_string (_isdcf_date));
	root->add_child("AudioChannels")->add_child_text (raw_convert<string> (_audio_channels));
	root->add_child("ThreeD")->add_child_text (_three_d ? "1" : "0");
	root->add_child("Sequence")->add_child_text (_sequence ? "1" : "0");
	root->add_child("Interop")->add_child_text (_interop ? "1" : "0");
	root->add_child("Encrypted")->add_child_text (_encrypted ? "1" : "0");
	root->add_child("Key")->add_child_text (_key.hex ());
	root->add_child("ContextID")->add_child_text (_context_id);
	if (_audio_processor) {
		root->add_child("AudioProcessor")->add_child_text (_audio_processor->id ());
	}
	root->add_child("ReelType")->add_child_text (raw_convert<string> (static_cast<int> (_reel_type)));
	root->add_child("ReelLength")->add_child_text (raw_convert<string> (_reel_length));
	root->add_child("ReencodeJ2K")->add_child_text (_reencode_j2k ? "1" : "0");
	root->add_child("UserExplicitVideoFrameRate")->add_child_text(_user_explicit_video_frame_rate ? "1" : "0");
	for (map<dcp::Marker, DCPTime>::const_iterator i = _markers.begin(); i != _markers.end(); ++i) {
		auto m = root->add_child("Marker");
		m->set_attribute("Type", dcp::marker_to_string(i->first));
		m->add_child_text(raw_convert<string>(i->second.get()));
	}
	for (auto i: _ratings) {
		i.as_xml (root->add_child("Rating"));
	}
	for (auto i: _content_versions) {
		root->add_child("ContentVersion")->add_child_text(i);
	}
	root->add_child("NameLanguage")->add_child_text(_name_language.to_string());
	if (_release_territory) {
		root->add_child("ReleaseTerritory")->add_child_text(_release_territory->subtag());
	}
	if (_sign_language_video_language) {
		root->add_child("SignLanguageVideoLanguage")->add_child_text(_sign_language_video_language->to_string());
	}
	root->add_child("VersionNumber")->add_child_text(raw_convert<string>(_version_number));
	root->add_child("Status")->add_child_text(dcp::status_to_string(_status));
	if (_chain) {
		root->add_child("Chain")->add_child_text(*_chain);
	}
	if (_distributor) {
		root->add_child("Distributor")->add_child_text(*_distributor);
	}
	if (_facility) {
		root->add_child("Facility")->add_child_text(*_facility);
	}
	if (_studio) {
		root->add_child("Studio")->add_child_text(*_studio);
	}
	root->add_child("TempVersion")->add_child_text(_temp_version ? "1" : "0");
	root->add_child("PreRelease")->add_child_text(_pre_release ? "1" : "0");
	root->add_child("RedBand")->add_child_text(_red_band ? "1" : "0");
	root->add_child("TwoDVersionOfThreeD")->add_child_text(_two_d_version_of_three_d ? "1" : "0");
	if (_luminance) {
		root->add_child("LuminanceValue")->add_child_text(raw_convert<string>(_luminance->value()));
		root->add_child("LuminanceUnit")->add_child_text(dcp::Luminance::unit_to_string(_luminance->unit()));
	}
	root->add_child("UserExplicitContainer")->add_child_text(_user_explicit_container ? "1" : "0");
	root->add_child("UserExplicitResolution")->add_child_text(_user_explicit_resolution ? "1" : "0");
	if (_audio_language) {
		root->add_child("AudioLanguage")->add_child_text(_audio_language->to_string());
	}
	_playlist->as_xml (root->add_child ("Playlist"), with_content_paths);

	return doc;
}

void
Film::write_metadata (boost::filesystem::path path) const
{
	metadata()->write_to_file_formatted(path.string());
}

/** Write state to our `metadata' file */
void
Film::write_metadata ()
{
	DCPOMATIC_ASSERT (directory());
	boost::filesystem::create_directories (directory().get());
	metadata()->write_to_file_formatted(file(metadata_file).string());
	set_dirty (false);
}

/** Write a template from this film */
void
Film::write_template (boost::filesystem::path path) const
{
	boost::filesystem::create_directories (path.parent_path());
	shared_ptr<xmlpp::Document> doc = metadata (false);
	metadata(false)->write_to_file_formatted(path.string());
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

	if (!boost::filesystem::exists(*path)) {
		throw FileNotFoundError(*path);
	}

	cxml::Document f ("Metadata");
	f.read_file (path.get ());

	_state_version = f.number_child<int> ("Version");
	if (_state_version > current_state_version) {
		throw runtime_error (_("This film was created with a newer version of DCP-o-matic, and it cannot be loaded into this version.  Sorry!"));
	} else if (_state_version < current_state_version) {
		/* This is an older version; save a copy (if we haven't already) */
		auto const older = path->parent_path() / String::compose("metadata.%1.xml", _state_version);
		if (!boost::filesystem::is_regular_file(older)) {
			try {
				boost::filesystem::copy_file(*path, older);
			} catch (...) {
				/* Never mind; at least we tried */
			}
		}
	}

	_last_written_by = f.optional_string_child("LastWrittenBy");

	_name = f.string_child ("Name");
	if (_state_version >= 9) {
		_use_isdcf_name = f.bool_child ("UseISDCFName");
		_isdcf_date = boost::gregorian::from_undelimited_string (f.string_child ("ISDCFDate"));
	} else {
		_use_isdcf_name = f.bool_child ("UseDCIName");
		_isdcf_date = boost::gregorian::from_undelimited_string (f.string_child ("DCIDate"));
	}


	{
		auto c = f.optional_string_child("DCPContentType");
		if (c) {
			_dcp_content_type = DCPContentType::from_isdcf_name (c.get ());
		}
	}

	{
		auto c = f.optional_string_child("Container");
		if (c) {
			_container = Ratio::from_id (c.get ());
		}
	}

	_resolution = string_to_resolution (f.string_child ("Resolution"));
	_j2k_bandwidth = f.number_child<int> ("J2KBandwidth");
	_video_frame_rate = f.number_child<int> ("VideoFrameRate");
	_audio_frame_rate = f.optional_number_child<int>("AudioFrameRate").get_value_or(48000);
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

	if (_audio_processor && !Config::instance()->show_experimental_audio_processors()) {
		auto ap = AudioProcessor::visible();
		if (find(ap.begin(), ap.end(), _audio_processor) == ap.end()) {
			Config::instance()->set_show_experimental_audio_processors(true);
		}
	}

	_reel_type = static_cast<ReelType> (f.optional_number_child<int>("ReelType").get_value_or (static_cast<int>(ReelType::SINGLE)));
	_reel_length = f.optional_number_child<int64_t>("ReelLength").get_value_or (2000000000);
	_reencode_j2k = f.optional_bool_child("ReencodeJ2K").get_value_or(false);
	_user_explicit_video_frame_rate = f.optional_bool_child("UserExplicitVideoFrameRate").get_value_or(false);

	for (auto i: f.node_children("Marker")) {
		_markers[dcp::marker_from_string(i->string_attribute("Type"))] = DCPTime(dcp::raw_convert<DCPTime::Type>(i->content()));
	}

	for (auto i: f.node_children("Rating")) {
		_ratings.push_back (dcp::Rating(i));
	}

	for (auto i: f.node_children("ContentVersion")) {
		_content_versions.push_back (i->content());
	}

	auto name_language = f.optional_string_child("NameLanguage");
	if (name_language) {
		_name_language = dcp::LanguageTag (*name_language);
	}
	auto release_territory = f.optional_string_child("ReleaseTerritory");
	if (release_territory) {
		_release_territory = dcp::LanguageTag::RegionSubtag (*release_territory);
	}

	auto sign_language_video_language = f.optional_string_child("SignLanguageVideoLanguage");
	if (sign_language_video_language) {
		_sign_language_video_language = dcp::LanguageTag(*sign_language_video_language);
	}

	_version_number = f.optional_number_child<int>("VersionNumber").get_value_or(0);

	auto status = f.optional_string_child("Status");
	if (status) {
		_status = dcp::string_to_status (*status);
	}

	_chain = f.optional_string_child("Chain");
	_distributor = f.optional_string_child("Distributor");
	_facility = f.optional_string_child("Facility");
	_studio = f.optional_string_child("Studio");
	_temp_version = f.optional_bool_child("TempVersion").get_value_or(false);
	_pre_release = f.optional_bool_child("PreRelease").get_value_or(false);
	_red_band = f.optional_bool_child("RedBand").get_value_or(false);
	_two_d_version_of_three_d = f.optional_bool_child("TwoDVersionOfThreeD").get_value_or(false);

	auto value = f.optional_number_child<float>("LuminanceValue");
	auto unit = f.optional_string_child("LuminanceUnit");
	if (value && unit) {
		_luminance = dcp::Luminance (*value, dcp::Luminance::string_to_unit(*unit));
	}

	/* Disable guessing for files made in previous DCP-o-matic versions */
	_user_explicit_container = f.optional_bool_child("UserExplicitContainer").get_value_or(true);
	_user_explicit_resolution = f.optional_bool_child("UserExplicitResolution").get_value_or(true);

	auto audio_language = f.optional_string_child("AudioLanguage");
	if (audio_language) {
		_audio_language = dcp::LanguageTag(*audio_language);
	}

	/* Read the old ISDCFMetadata tag from 2.14.x metadata */
	auto isdcf = f.optional_node_child("ISDCFMetadata");
	if (isdcf) {
		if (auto territory = isdcf->optional_string_child("Territory")) {
			try {
				_release_territory = dcp::LanguageTag::RegionSubtag(*territory);
			} catch (...) {
				/* Invalid region subtag; just ignore it */
			}
		}
		if (auto audio_language = isdcf->optional_string_child("AudioLanguage")) {
			try {
				_audio_language = dcp::LanguageTag(*audio_language);
			} catch (...) {
				/* Invalid language tag; just ignore it */
			}
		}
		if (auto content_version = isdcf->optional_string_child("ContentVersion")) {
			_content_versions.push_back (*content_version);
		}
		if (auto rating = isdcf->optional_string_child("Rating")) {
			_ratings.push_back (dcp::Rating("", *rating));
		}
		if (auto mastered_luminance = isdcf->optional_number_child<float>("MasteredLuminance")) {
			if (*mastered_luminance > 0) {
				_luminance = dcp::Luminance(*mastered_luminance, dcp::Luminance::Unit::FOOT_LAMBERT);
			}
		}
		_studio = isdcf->optional_string_child("Studio");
		_facility = isdcf->optional_string_child("Facility");
		_temp_version = isdcf->optional_bool_child("TempVersion").get_value_or("false");
		_pre_release = isdcf->optional_bool_child("PreRelease").get_value_or("false");
		_red_band = isdcf->optional_bool_child("RedBand").get_value_or("false");
		_two_d_version_of_three_d = isdcf->optional_bool_child("TwoDVersionOfThreeD").get_value_or("false");
		_chain = isdcf->optional_string_child("Chain");
	}

	list<string> notes;
	_playlist->set_from_xml (shared_from_this(), f.node_child ("Playlist"), _state_version, notes);

	/* Write backtraces to this film's directory, until another film is loaded */
	if (_directory) {
		set_backtrace_file (file ("backtrace.txt"));
	}

	set_dirty (false);
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
		for (auto i: content()) {
			if (i->audio) {
				auto c = i->audio->mapping().mapped_output_channels();
				copy (c.begin(), c.end(), back_inserter(mapped));
			}
		}

		mapped.sort ();
		mapped.unique ();
	}

	return mapped;
}


pair<optional<dcp::LanguageTag>, vector<dcp::LanguageTag>>
Film::subtitle_languages () const
{
	pair<optional<dcp::LanguageTag>, vector<dcp::LanguageTag>> result;
	for (auto i: content()) {
		for (auto j: i->text) {
			if (j->use() && j->type() == TextType::OPEN_SUBTITLE && j->language()) {
				if (j->language_is_additional()) {
					result.second.push_back (j->language().get());
				} else {
					result.first = j->language().get();
				}
			}
		}
		if (i->video && i->video->burnt_subtitle_language()) {
			result.first = i->video->burnt_subtitle_language();
		}
	}

	std::sort (result.second.begin(), result.second.end());
	auto last = std::unique (result.second.begin(), result.second.end());
	result.second.erase (last, result.second.end());
	return result;
}


/** @return a ISDCF-compliant name for a DCP of this film */
string
Film::isdcf_name (bool if_created_now) const
{
	string isdcf_name;

	auto raw_name = name ();

	/* Split the raw name up into words */
	vector<string> words;
	split (words, raw_name, is_any_of (" _-"));

	string fixed_name;

	/* Add each word to fixed_name */
	for (auto i: words) {
		string w = i;

		/* First letter is always capitalised */
		w[0] = toupper (w[0]);

		/* Count caps in w */
		size_t caps = 0;
		for (size_t j = 0; j < w.size(); ++j) {
			if (isupper (w[j])) {
				++caps;
			}
		}

		/* If w is all caps make the rest of it lower case, otherwise
		   leave it alone.
		*/
		if (caps == w.size ()) {
			for (size_t j = 1; j < w.size(); ++j) {
				w[j] = tolower (w[j]);
			}
		}

		for (size_t j = 0; j < w.size(); ++j) {
			fixed_name += w[j];
		}
	}

	if (fixed_name.length() > 14) {
		fixed_name = fixed_name.substr (0, 14);
	}

	isdcf_name += fixed_name;

	if (dcp_content_type()) {
		isdcf_name += "_" + dcp_content_type()->isdcf_name();
		string version = "1";
		if (_interop) {
			if (!_content_versions.empty()) {
				auto cv = _content_versions[0];
				if (!cv.empty() && std::all_of(cv.begin(), cv.end(), isdigit)) {
					version = cv;
				}
			}
		} else {
			version = dcp::raw_convert<string>(_version_number);
		}
		isdcf_name += "-" + version;
	}

	if (_temp_version) {
		isdcf_name += "-Temp";
	}

	if (_pre_release) {
		isdcf_name += "-Pre";
	}

	if (_red_band) {
		isdcf_name += "-RedBand";
	}

	if (_chain && !_chain->empty()) {
		isdcf_name += "-" + *_chain;
	}

	if (three_d ()) {
		isdcf_name += "-3D";
	}

	if (_two_d_version_of_three_d) {
		isdcf_name += "-2D";
	}

	if (_luminance) {
		auto fl = _luminance->value_in_foot_lamberts();
		char buffer[64];
		snprintf (buffer, sizeof(buffer), "%.1f", fl);
		isdcf_name += String::compose("-%1fl", buffer);
	}

	if (video_frame_rate() != 24) {
		isdcf_name += "-" + raw_convert<string>(video_frame_rate());
	}

	if (container()) {
		isdcf_name += "_" + container()->isdcf_name();
	}

	/* XXX: this uses the first bit of content only */

	/* Interior aspect ratio.  The standard says we don't do this for trailers, for some strange reason */
	if (dcp_content_type() && dcp_content_type()->libdcp_kind() != dcp::ContentKind::TRAILER) {
		auto cl = content();
		auto first_video = std::find_if(cl.begin(), cl.end(), [](shared_ptr<Content> c) { return static_cast<bool>(c->video); });
		if (first_video != cl.end()) {
			auto first_ratio = lrintf((*first_video)->video->scaled_size(frame_size()).ratio() * 100);
			auto container_ratio = lrintf(container()->ratio() * 100);
			if (first_ratio != container_ratio) {
				isdcf_name += "-" + dcp::raw_convert<string>(first_ratio);
			}
		}
	}

	auto entry_for_language = [](dcp::LanguageTag const& tag) {
		/* Look up what we should be using for this tag in the DCNC name */
		for (auto const& dcnc: dcp::dcnc_tags()) {
			if (tag.to_string() == dcnc.first) {
				return dcnc.second;
			}
		}
		/* Fallback to the language subtag, if there is one */
		return tag.language() ? tag.language()->subtag() : "XX";
	};

	auto audio_language = _audio_language ? entry_for_language(*_audio_language) : "XX";

	isdcf_name += "_" + to_upper (audio_language);

	/* I'm not clear on the precise details of the convention for CCAP labelling;
	   for now I'm just appending -CCAP if we have any closed captions.
	*/

	auto burnt_in = true;
	auto ccap = false;
	for (auto i: content()) {
		for (auto j: i->text) {
			if (j->type() == TextType::OPEN_SUBTITLE && j->use() && !j->burn()) {
				burnt_in = false;
			} else if (j->type() == TextType::CLOSED_CAPTION && j->use()) {
				ccap = true;
			}
		}
	}

	auto sub_langs = subtitle_languages();
	if (sub_langs.first && sub_langs.first->language()) {
		auto lang = entry_for_language(*sub_langs.first);
		if (burnt_in) {
			transform (lang.begin(), lang.end(), lang.begin(), ::tolower);
		} else {
			lang = to_upper (lang);
		}

		isdcf_name += "-" + lang;
		if (ccap) {
			isdcf_name += "-CCAP";
		}
	} else {
		/* No subtitles */
		isdcf_name += "-XX";
	}

	if (_release_territory) {
		auto territory = _release_territory->subtag();
		isdcf_name += "_" + to_upper (territory);
		if (_ratings.empty ()) {
			isdcf_name += "-NR";
		} else {
			auto label = _ratings[0].label;
			boost::erase_all(label, "+");
			boost::erase_all(label, "-");
			isdcf_name += "-" + label;
		}
	}

	/* Count mapped audio channels */

	auto mapped = mapped_audio_channels ();

	auto ch = audio_channel_types (mapped, audio_channels());
	if (!ch.first && !ch.second) {
		isdcf_name += "_MOS";
	} else if (ch.first) {
		isdcf_name += String::compose("_%1%2", ch.first, ch.second);
	}

	if (audio_channels() > static_cast<int>(dcp::Channel::HI) && find(mapped.begin(), mapped.end(), static_cast<int>(dcp::Channel::HI)) != mapped.end()) {
		isdcf_name += "-HI";
	}
	if (audio_channels() > static_cast<int>(dcp::Channel::VI) && find(mapped.begin(), mapped.end(), static_cast<int>(dcp::Channel::VI)) != mapped.end()) {
		isdcf_name += "-VI";
	}

	isdcf_name += "_" + resolution_to_string (_resolution);

	if (_studio && _studio->length() >= 2) {
		isdcf_name += "_" + to_upper (_studio->substr(0, 4));
	}

	if (if_created_now) {
		isdcf_name += "_" + boost::gregorian::to_iso_string (boost::gregorian::day_clock::local_day ());
	} else {
		isdcf_name += "_" + boost::gregorian::to_iso_string (_isdcf_date);
	}

	if (_facility && _facility->length() >= 3) {
		isdcf_name += "_" + to_upper(_facility->substr(0, 3));
	}

	if (_interop) {
		isdcf_name += "_IOP";
	} else {
		isdcf_name += "_SMPTE";
	}

	if (three_d ()) {
		isdcf_name += "-3D";
	}

	auto vf = false;
	for (auto i: content()) {
		auto dc = dynamic_pointer_cast<const DCPContent>(i);
		if (!dc) {
			continue;
		}

		bool any_text = false;
		for (int i = 0; i < static_cast<int>(TextType::COUNT); ++i) {
			if (dc->reference_text(static_cast<TextType>(i))) {
				any_text = true;
			}
		}
		if (dc->reference_video() || dc->reference_audio() || any_text) {
			vf = true;
		}
	}

	if (vf) {
		isdcf_name += "_VF";
	} else {
		isdcf_name += "_OV";
	}

	return isdcf_name;
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
	set_dirty (true);
}

void
Film::set_name (string n)
{
	FilmChangeSignaller ch (this, Property::NAME);
	_name = n;
}

void
Film::set_use_isdcf_name (bool u)
{
	FilmChangeSignaller ch (this, Property::USE_ISDCF_NAME);
	_use_isdcf_name = u;
}

void
Film::set_dcp_content_type (DCPContentType const * t)
{
	FilmChangeSignaller ch (this, Property::DCP_CONTENT_TYPE);
	_dcp_content_type = t;
}


/** @param explicit_user true if this is being set because of
 *  a direct user request, false if it is being done because
 *  DCP-o-matic is guessing the best container to use.
 */
void
Film::set_container (Ratio const * c, bool explicit_user)
{
	FilmChangeSignaller ch (this, Property::CONTAINER);
	_container = c;

	if (explicit_user) {
		_user_explicit_container = true;
	}
}


/** @param explicit_user true if this is being set because of
 *  a direct user request, false if it is being done because
 *  DCP-o-matic is guessing the best resolution to use.
 */
void
Film::set_resolution (Resolution r, bool explicit_user)
{
	FilmChangeSignaller ch (this, Property::RESOLUTION);
	_resolution = r;

	if (explicit_user) {
		_user_explicit_resolution = true;
	}
}


void
Film::set_j2k_bandwidth (int b)
{
	FilmChangeSignaller ch (this, Property::J2K_BANDWIDTH);
	_j2k_bandwidth = b;
}

/** @param f New frame rate.
 *  @param user_explicit true if this comes from a direct user instruction, false if it is from
 *  DCP-o-matic being helpful.
 */
void
Film::set_video_frame_rate (int f, bool user_explicit)
{
	FilmChangeSignaller ch (this, Property::VIDEO_FRAME_RATE);
	_video_frame_rate = f;
	if (user_explicit) {
		_user_explicit_video_frame_rate = true;
	}
}

void
Film::set_audio_channels (int c)
{
	FilmChangeSignaller ch (this, Property::AUDIO_CHANNELS);
	_audio_channels = c;
}

void
Film::set_three_d (bool t)
{
	FilmChangeSignaller ch (this, Property::THREE_D);
	_three_d = t;

	if (_three_d && _two_d_version_of_three_d) {
		set_two_d_version_of_three_d (false);
	}
}

void
Film::set_interop (bool i)
{
	FilmChangeSignaller ch (this, Property::INTEROP);
	_interop = i;
}

void
Film::set_audio_processor (AudioProcessor const * processor)
{
	FilmChangeSignaller ch1 (this, Property::AUDIO_PROCESSOR);
	FilmChangeSignaller ch2 (this, Property::AUDIO_CHANNELS);
	_audio_processor = processor;
}

void
Film::set_reel_type (ReelType t)
{
	FilmChangeSignaller ch (this, Property::REEL_TYPE);
	_reel_type = t;
}

/** @param r Desired reel length in bytes */
void
Film::set_reel_length (int64_t r)
{
	FilmChangeSignaller ch (this, Property::REEL_LENGTH);
	_reel_length = r;
}

void
Film::set_reencode_j2k (bool r)
{
	FilmChangeSignaller ch (this, Property::REENCODE_J2K);
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
	if (type == ChangeType::DONE) {
		set_dirty (true);

		if (p == Property::CONTENT) {
			if (!_user_explicit_video_frame_rate) {
				set_video_frame_rate (best_video_frame_rate());
			}
		}

		emit (boost::bind (boost::ref (Change), type, p));

		if (p == Property::VIDEO_FRAME_RATE || p == Property::SEQUENCE) {
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

	if (eyes == Eyes::LEFT) {
		s += ".L";
	} else if (eyes == Eyes::RIGHT) {
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

	auto const dir = directory().get();
	for (auto i: boost::filesystem::directory_iterator(dir)) {
		if (
			boost::filesystem::is_directory (i) &&
			i.path().leaf() != "j2c" && i.path().leaf() != "video" && i.path().leaf() != "info" && i.path().leaf() != "analysis"
			) {

			try {
				out.push_back (CPLSummary(i));
			} catch (...) {

			}
		}
	}

	sort (out.begin(), out.end(), cpl_summary_compare);

	return out;
}

void
Film::set_encrypted (bool e)
{
	FilmChangeSignaller ch (this, Property::ENCRYPTED);
	_encrypted = e;
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

	auto j = make_shared<ExamineContentJob>(shared_from_this(), content);

	_job_connections.push_back (
		j->Finished.connect (bind (&Film::maybe_add_content, this, weak_ptr<Job>(j), weak_ptr<Content>(content), disable_audio_analysis))
		);

	JobManager::instance()->add (j);
}

void
Film::maybe_add_content (weak_ptr<Job> j, weak_ptr<Content> c, bool disable_audio_analysis)
{
	auto job = j.lock ();
	if (!job || !job->finished_ok ()) {
		return;
	}

	auto content = c.lock ();
	if (!content) {
		return;
	}

	add_content (content);

	if (Config::instance()->automatic_audio_analysis() && content->audio && !disable_audio_analysis) {
		auto playlist = make_shared<Playlist>();
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
		for (auto i: _template_film->content()) {
			c->take_settings_from (i);
		}
	}

	_playlist->add (shared_from_this(), c);

	maybe_set_container_and_resolution ();
	if (c->atmos) {
		set_audio_channels (14);
		set_interop (false);
	}
}


void
Film::maybe_set_container_and_resolution ()
{
	/* Get the only piece of video content, if there is only one */
	shared_ptr<VideoContent> video;
	for (auto i: _playlist->content()) {
		if (i->video) {
			if (!video) {
				video = i->video;
			} else {
				video.reset ();
			}
		}
	}

	if (video) {
		/* This is the only piece of video content in this Film.  Use it to make a guess for
		 * DCP container size and resolution, unless the user has already explicitly set these
		 * things.
		 */
		if (!_user_explicit_container) {
			if (video->size().ratio() > 2.3) {
				set_container (Ratio::from_id("239"), false);
			} else {
				set_container (Ratio::from_id("185"), false);
			}
		}

		if (!_user_explicit_resolution) {
			if (video->size_after_crop().width > 2048 || video->size_after_crop().height > 1080) {
				set_resolution (Resolution::FOUR_K, false);
			} else {
				set_resolution (Resolution::TWO_K, false);
			}
		}
	}
}

void
Film::remove_content (shared_ptr<Content> c)
{
	_playlist->remove (c);
	maybe_set_container_and_resolution ();
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

/** @return length of the film from time 0 to the last thing on the playlist,
 *  with a minimum length of 1 second.
 */
DCPTime
Film::length () const
{
	return max(DCPTime::from_seconds(1), _playlist->length(shared_from_this()).ceil(video_frame_rate()));
}

int
Film::best_video_frame_rate () const
{
	/* Don't default to anything above 30fps (make the user select that explicitly) */
	auto best = _playlist->best_video_frame_rate ();
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
		signal_change (type, Property::CONTENT);
	} else if (p == AudioContentProperty::STREAMS) {
		signal_change (type, Property::NAME);
	}

	if (type == ChangeType::DONE) {
		emit (boost::bind (boost::ref (ContentChange), type, c, p, frequent));
		if (!frequent) {
			check_settings_consistency ();
		}
	} else {
		ContentChange (type, c, p, frequent);
	}

	set_dirty (true);
}

void
Film::playlist_length_change ()
{
	LengthChange ();
}

void
Film::playlist_change (ChangeType type)
{
	signal_change (type, Property::CONTENT);
	signal_change (type, Property::NAME);

	if (type == ChangeType::DONE) {
		check_settings_consistency ();
	}

	set_dirty (true);
}

/** Check for (and if necessary fix) impossible settings combinations, like
 *  video set to being referenced when it can't be.
 */
void
Film::check_settings_consistency ()
{
	optional<int> atmos_rate;
	for (auto i: content()) {

		if (i->atmos) {
			int rate = lrintf (i->atmos->edit_rate().as_float());
			if (atmos_rate && *atmos_rate != rate) {
				Message (_("You have more than one piece of Atmos content, and they do not have the same frame rate.  You must remove some Atmos content."));
			} else if (!atmos_rate && rate != video_frame_rate()) {
				atmos_rate = rate;
				set_video_frame_rate (rate, false);
				Message (_("DCP-o-matic had to change your settings so that the film's frame rate is the same as that of your Atmos content."));
			}
		}
	}

	bool change_made = false;
	for (auto i: content()) {
		auto d = dynamic_pointer_cast<DCPContent>(i);
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
		if (d->reference_text(TextType::OPEN_SUBTITLE) && !d->can_reference_text(shared_from_this(), TextType::OPEN_SUBTITLE, why_not)) {
			d->set_reference_text(TextType::OPEN_SUBTITLE, false);
			change_made = true;
		}
		if (d->reference_text(TextType::CLOSED_CAPTION) && !d->can_reference_text(shared_from_this(), TextType::CLOSED_CAPTION, why_not)) {
			d->set_reference_text(TextType::CLOSED_CAPTION, false);
			change_made = true;
		}
	}

	if (change_made) {
		Message (_("DCP-o-matic had to change your settings for referring to DCPs as OV.  Please review those settings to make sure they are what you want."));
	}
}

void
Film::playlist_order_changed ()
{
	/* XXX: missing PENDING */
	signal_change (ChangeType::DONE, Property::CONTENT_ORDER);
}


void
Film::set_sequence (bool s)
{
	if (s == _sequence) {
		return;
	}

	FilmChangeSignaller cc (this, Property::SEQUENCE);
	_sequence = s;
	_playlist->set_sequence (s);
}

/** @return Size of the largest possible image in whatever resolution we are using */
dcp::Size
Film::full_frame () const
{
	switch (_resolution) {
	case Resolution::TWO_K:
		return dcp::Size (2048, 1080);
	case Resolution::FOUR_K:
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


/** @return Area of Film::frame_size() that contains picture rather than pillar/letterboxing */
dcp::Size
Film::active_area () const
{
	auto const frame = frame_size ();
	dcp::Size active;

	for (auto i: content()) {
		if (i->video) {
			dcp::Size s = i->video->scaled_size (frame);
			active.width = max(active.width, s.width);
			active.height = max(active.height, s.height);
		}
	}

	return active;
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

	auto cpl = make_shared<dcp::CPL>(cpl_file);
	auto signer = Config::instance()->signer_chain();
	if (!signer->valid ()) {
		throw InvalidSignerError ();
	}

	/* Find keys that have been added to imported, encrypted DCP content */
	list<dcp::DecryptedKDMKey> imported_keys;
	for (auto i: content()) {
		auto d = dynamic_pointer_cast<DCPContent> (i);
		if (d && d->kdm()) {
			dcp::DecryptedKDM kdm (d->kdm().get(), Config::instance()->decryption_chain()->key().get());
			auto keys = kdm.keys ();
			copy (keys.begin(), keys.end(), back_inserter (imported_keys));
		}
	}

	map<shared_ptr<const dcp::ReelFileAsset>, dcp::Key> keys;

	for (auto i: cpl->reel_file_assets()) {
		if (!i->encrypted()) {
			continue;
		}

		/* Get any imported key for this ID */
		bool done = false;
		for (auto j: imported_keys) {
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
	dcp::File f(test, "w");
	if (f) {
		f.close();
		boost::system::error_code ec;
		boost::filesystem::create_hard_link (test, test2, ec);
		if (ec) {
			can_hard_link = false;
		}
		boost::filesystem::remove (test);
		boost::filesystem::remove (test2);
	}

	auto s = boost::filesystem::space (internal_video_asset_dir ());
	required = double (required_disk_space ()) / 1073741824.0f;
	if (!can_hard_link) {
		required *= 2;
	}
	available = double (s.available) / 1073741824.0f;
	return (available - required) > 1;
}

/** @return The names of the channels that audio contents' outputs are passed into;
 *  this is either the DCP or a AudioProcessor.
 */
vector<NamedChannel>
Film::audio_output_names () const
{
	if (audio_processor ()) {
		return audio_processor()->input_names ();
	}

	DCPOMATIC_ASSERT (MAX_DCP_AUDIO_CHANNELS == 16);

	vector<NamedChannel> n;

	for (int i = 0; i < audio_channels(); ++i) {
		if (Config::instance()->use_all_audio_channels() || (i != 8 && i != 9 && i != 15)) {
			n.push_back (NamedChannel(short_audio_channel_name(i), i));
		}
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
	auto const len = length();

	switch (reel_type ()) {
	case ReelType::SINGLE:
		p.push_back (DCPTimePeriod (DCPTime (), len));
		break;
	case ReelType::BY_VIDEO_CONTENT:
	{
		/* Collect all reel boundaries */
		list<DCPTime> split_points;
		split_points.push_back (DCPTime());
		split_points.push_back (len);
		for (auto c: content()) {
			if (c->video) {
				for (auto t: c->reel_split_points(shared_from_this())) {
					split_points.push_back (t);
				}
				split_points.push_back (c->end(shared_from_this()));
			}
		}

		split_points.sort ();
		split_points.unique ();

		/* Make them into periods, coalescing any that are less than 1 second long */
		optional<DCPTime> last;
		for (auto t: split_points) {
			if (last && (t - *last) >= DCPTime::from_seconds(1)) {
				/* Period from *last to t is long enough; use it and start a new one */
				p.push_back (DCPTimePeriod(*last, t));
				last = t;
			} else if (!last) {
				/* That was the first time, so start a new period */
				last = t;
			}
		}

		if (!p.empty()) {
			p.back().to = split_points.back();
		}
		break;
	}
	case ReelType::BY_LENGTH:
	{
		DCPTime current;
		/* Integer-divide reel length by the size of one frame to give the number of frames per reel,
		 * making sure we don't go less than 1s long.
		 */
		Frame const reel_in_frames = max(_reel_length / ((j2k_bandwidth() / video_frame_rate()) / 8), static_cast<Frame>(video_frame_rate()));
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
	_template_film->read_metadata (Config::instance()->template_read_path(name));
	_use_isdcf_name = _template_film->_use_isdcf_name;
	_dcp_content_type = _template_film->_dcp_content_type;
	_container = _template_film->_container;
	_resolution = _template_film->_resolution;
	_j2k_bandwidth = _template_film->_j2k_bandwidth;
	_video_frame_rate = _template_film->_video_frame_rate;
	_encrypted = _template_film->_encrypted;
	_audio_channels = _template_film->_audio_channels;
	_sequence = _template_film->_sequence;
	_three_d = _template_film->_three_d;
	_interop = _template_film->_interop;
	_audio_processor = _template_film->_audio_processor;
	_reel_type = _template_film->_reel_type;
	_reel_length = _template_film->_reel_length;
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
	for (auto i: _playlist->content()) {
		auto d = dynamic_pointer_cast<DCPContent>(i);
		if (d && d->reference_video()) {
			return true;
		}
	}

	return false;
}

bool
Film::references_dcp_audio () const
{
	for (auto i: _playlist->content()) {
		auto d = dynamic_pointer_cast<DCPContent>(i);
		if (d && d->reference_audio()) {
			return true;
		}
	}

	return false;
}


bool
Film::contains_atmos_content () const
{
	for (auto i: _playlist->content()) {
		if (i->atmos) {
			return true;
		}
	}

	return false;
}


list<DCPTextTrack>
Film::closed_caption_tracks () const
{
	list<DCPTextTrack> tt;
	for (auto i: content()) {
		for (auto j: i->text) {
			/* XXX: Empty DCPTextTrack ends up being a magic value here - the "unknown" or "not specified" track */
			auto dtt = j->dcp_track().get_value_or(DCPTextTrack());
			if (j->type() == TextType::CLOSED_CAPTION && find(tt.begin(), tt.end(), dtt) == tt.end()) {
				tt.push_back (dtt);
			}
		}
	}

	return tt;
}

void
Film::set_marker (dcp::Marker type, DCPTime time)
{
	FilmChangeSignaller ch (this, Property::MARKERS);
	_markers[type] = time;
}


void
Film::unset_marker (dcp::Marker type)
{
	FilmChangeSignaller ch (this, Property::MARKERS);
	_markers.erase (type);
}


void
Film::clear_markers ()
{
	FilmChangeSignaller ch (this, Property::MARKERS);
	_markers.clear ();
}


void
Film::set_ratings (vector<dcp::Rating> r)
{
	FilmChangeSignaller ch (this, Property::RATINGS);
	_ratings = r;
}

void
Film::set_content_versions (vector<string> v)
{
	FilmChangeSignaller ch (this, Property::CONTENT_VERSIONS);
	_content_versions = v;
}


void
Film::set_name_language (dcp::LanguageTag lang)
{
	FilmChangeSignaller ch (this, Property::NAME_LANGUAGE);
	_name_language = lang;
}


void
Film::set_release_territory (optional<dcp::LanguageTag::RegionSubtag> region)
{
	FilmChangeSignaller ch (this, Property::RELEASE_TERRITORY);
	_release_territory = region;
}


void
Film::set_status (dcp::Status s)
{
	FilmChangeSignaller ch (this, Property::STATUS);
	_status = s;
}


void
Film::set_version_number (int v)
{
	FilmChangeSignaller ch (this, Property::VERSION_NUMBER);
	_version_number = v;
}


void
Film::set_chain (optional<string> c)
{
	FilmChangeSignaller ch (this, Property::CHAIN);
	_chain = c;
}


void
Film::set_distributor (optional<string> d)
{
	FilmChangeSignaller ch (this, Property::DISTRIBUTOR);
	_distributor = d;
}


void
Film::set_luminance (optional<dcp::Luminance> l)
{
	FilmChangeSignaller ch (this, Property::LUMINANCE);
	_luminance = l;
}


void
Film::set_facility (optional<string> f)
{
	FilmChangeSignaller ch (this, Property::FACILITY);
	_facility = f;
}


void
Film::set_studio (optional<string> s)
{
	FilmChangeSignaller ch (this, Property::STUDIO);
	_studio = s;
}


optional<DCPTime>
Film::marker (dcp::Marker type) const
{
	auto i = _markers.find (type);
	if (i == _markers.end()) {
		return {};
	}
	return i->second;
}

shared_ptr<InfoFileHandle>
Film::info_file_handle (DCPTimePeriod period, bool read) const
{
	return std::make_shared<InfoFileHandle>(_info_file_mutex, info_file(period), read);
}

InfoFileHandle::InfoFileHandle (boost::mutex& mutex, boost::filesystem::path path, bool read)
	: _lock (mutex)
	, _file (path, read ? "rb" : (boost::filesystem::exists(path) ? "r+b" : "wb"))
{
	if (!_file) {
		throw OpenFileError (path, errno, read ? OpenFileError::READ : (boost::filesystem::exists(path) ? OpenFileError::READ_WRITE : OpenFileError::WRITE));
	}
}


/** Add FFOC and LFOC markers to a list if they are not already there */
void
Film::add_ffoc_lfoc (Markers& markers) const
{
	if (markers.find(dcp::Marker::FFOC) == markers.end()) {
		markers[dcp::Marker::FFOC] = dcpomatic::DCPTime::from_frames(1, video_frame_rate());
	}

	if (markers.find(dcp::Marker::LFOC) == markers.end()) {
		markers[dcp::Marker::LFOC] = length() - DCPTime::from_frames(1, video_frame_rate());
	}
}


void
Film::set_temp_version (bool t)
{
	FilmChangeSignaller ch (this, Property::TEMP_VERSION);
	_temp_version = t;
}


void
Film::set_pre_release (bool p)
{
	FilmChangeSignaller ch (this, Property::PRE_RELEASE);
	_pre_release = p;
}


void
Film::set_red_band (bool r)
{
	FilmChangeSignaller ch (this, Property::RED_BAND);
	_red_band = r;
}


void
Film::set_two_d_version_of_three_d (bool t)
{
	FilmChangeSignaller ch (this, Property::TWO_D_VERSION_OF_THREE_D);
	_two_d_version_of_three_d = t;
}


void
Film::set_audio_language (optional<dcp::LanguageTag> language)
{
	FilmChangeSignaller ch (this, Property::AUDIO_LANGUAGE);
	_audio_language = language;
}


void
Film::set_audio_frame_rate (int rate)
{
	FilmChangeSignaller ch (this, Property::AUDIO_FRAME_RATE);
	_audio_frame_rate = rate;
}


bool
Film::has_sign_language_video_channel () const
{
	return _audio_channels >= static_cast<int>(dcp::Channel::SIGN_LANGUAGE);
}


void
Film::set_sign_language_video_language (optional<dcp::LanguageTag> lang)
{
	FilmChangeSignaller ch (this, Property::SIGN_LANGUAGE_VIDEO_LANGUAGE);
	_sign_language_video_language = lang;
}


void
Film::set_dirty (bool dirty)
{
	auto const changed = dirty != _dirty;
	_dirty = dirty;
	if (changed) {
		emit (boost::bind(boost::ref(DirtyChange), _dirty));
	}
}


/** @return true if the metadata was (probably) last written by a version earlier
 *  than the given one; false if it definitely was not.
 */
bool
Film::last_written_by_earlier_than(int major, int minor, int micro) const
{
	if (!_last_written_by) {
		return true;
	}

	vector<string> parts;
	boost::split(parts, *_last_written_by, boost::is_any_of("."));

	if (parts.size() != 3) {
		/* Not sure what's going on, so let's say it was written by an old version */
		return true;
	}

	if (boost::ends_with(parts[2], "pre")) {
		parts[2] = parts[2].substr(0, parts[2].length() - 3);
	}

	int our_major = dcp::raw_convert<int>(parts[0]);
	int our_minor = dcp::raw_convert<int>(parts[1]);
	int our_micro = dcp::raw_convert<int>(parts[2]);

	if (our_major != major) {
		return our_major < major;
	}

	if (our_minor != minor) {
		return our_minor < minor;
	}

	return our_micro < micro;
}

