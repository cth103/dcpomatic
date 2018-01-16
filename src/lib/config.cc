/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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

#include "config.h"
#include "filter.h"
#include "ratio.h"
#include "types.h"
#include "log.h"
#include "dcp_content_type.h"
#include "cinema_sound_processor.h"
#include "colour_conversion.h"
#include "cinema.h"
#include "util.h"
#include "cross.h"
#include "film.h"
#include "dkdm_wrapper.h"
#include "compose.hpp"
#include <dcp/raw_convert.h>
#include <dcp/name_format.h>
#include <dcp/certificate_chain.h>
#include <libcxml/cxml.h>
#include <glib.h>
#include <libxml++/libxml++.h>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/thread.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "i18n.h"

using std::vector;
using std::cout;
using std::ifstream;
using std::string;
using std::list;
using std::max;
using std::remove;
using std::exception;
using std::cerr;
using boost::shared_ptr;
using boost::optional;
using boost::dynamic_pointer_cast;
using boost::algorithm::trim;
using dcp::raw_convert;

Config* Config::_instance = 0;
boost::signals2::signal<void ()> Config::FailedToLoad;
boost::signals2::signal<void (string)> Config::Warning;
boost::optional<boost::filesystem::path> Config::test_path;

/** Construct default configuration */
Config::Config ()
        /* DKDMs are not considered a thing to reset on set_defaults() */
	: _dkdms (new DKDMGroup ("root"))
{
	set_defaults ();
}

void
Config::set_defaults ()
{
	_master_encoding_threads = max (2U, boost::thread::hardware_concurrency ());
	_server_encoding_threads = max (2U, boost::thread::hardware_concurrency ());
	_server_port_base = 6192;
	_use_any_servers = true;
	_servers.clear ();
	_only_servers_encode = false;
	_tms_protocol = PROTOCOL_SCP;
	_tms_ip = "";
	_tms_path = ".";
	_tms_user = "";
	_tms_password = "";
	_cinema_sound_processor = CinemaSoundProcessor::from_id (N_("dolby_cp750"));
	_allow_any_dcp_frame_rate = false;
	_language = optional<string> ();
	_default_still_length = 10;
	_default_container = Ratio::from_id ("185");
	_default_scale_to = 0;
	_default_dcp_content_type = DCPContentType::from_isdcf_name ("FTR");
	_default_dcp_audio_channels = 6;
	_default_j2k_bandwidth = 100000000;
	_default_audio_delay = 0;
	_default_interop = true;
	_default_upload_after_make_dcp = false;
	_mail_server = "";
	_mail_port = 25;
	_mail_user = "";
	_mail_password = "";
	_kdm_from = "";
	_kdm_cc.clear ();
	_kdm_bcc = "";
	_check_for_updates = false;
	_check_for_test_updates = false;
	_maximum_j2k_bandwidth = 250000000;
	_log_types = LogEntry::TYPE_GENERAL | LogEntry::TYPE_WARNING | LogEntry::TYPE_ERROR;
	_analyse_ebur128 = true;
	_automatic_audio_analysis = false;
#ifdef DCPOMATIC_WINDOWS
	_win32_console = false;
#endif
	_cinemas_file = path ("cinemas.xml");
	_show_hints_before_make_dcp = true;
	_confirm_kdm_email = true;
	_kdm_container_name_format = dcp::NameFormat ("KDM %f %c");
	_kdm_filename_format = dcp::NameFormat ("KDM %f %c %s");
	_dcp_metadata_filename_format = dcp::NameFormat ("%t");
	_dcp_asset_filename_format = dcp::NameFormat ("%t");
	_jump_to_selected = true;
	for (int i = 0; i < NAG_COUNT; ++i) {
		_nagged[i] = false;
	}
	_sound = false;
	_sound_output = optional<string> ();
	_last_kdm_write_type = KDM_WRITE_FLAT;

	/* I think the scaling factor here should be the ratio of the longest frame
	   encode time to the shortest; if the thread count is T, longest time is L
	   and the shortest time S we could encode L/S frames per thread whilst waiting
	   for the L frame to encode so we might have to store LT/S frames.

	   However we don't want to use too much memory, so keep it a bit lower than we'd
	   perhaps like.  A J2K frame is typically about 1Mb so 3 here will mean we could
	   use about 240Mb with 72 encoding threads.
	*/
	_frames_in_memory_multiplier = 3;

	_allowed_dcp_frame_rates.clear ();
	_allowed_dcp_frame_rates.push_back (24);
	_allowed_dcp_frame_rates.push_back (25);
	_allowed_dcp_frame_rates.push_back (30);
	_allowed_dcp_frame_rates.push_back (48);
	_allowed_dcp_frame_rates.push_back (50);
	_allowed_dcp_frame_rates.push_back (60);

	set_kdm_email_to_default ();
	set_cover_sheet_to_default ();
}

void
Config::restore_defaults ()
{
	Config::instance()->set_defaults ();
	Config::instance()->changed ();
}

shared_ptr<dcp::CertificateChain>
Config::create_certificate_chain ()
{
	return shared_ptr<dcp::CertificateChain> (
		new dcp::CertificateChain (
			openssl_path(),
			"dcpomatic.com",
			"dcpomatic.com",
			".dcpomatic.smpte-430-2.ROOT",
			".dcpomatic.smpte-430-2.INTERMEDIATE",
			"CS.dcpomatic.smpte-430-2.LEAF"
			)
		);
}

void
Config::read ()
try
{
	cxml::Document f ("Config");
	f.read_file (config_file ());

	optional<int> version = f.optional_number_child<int> ("Version");

	if (f.optional_number_child<int>("NumLocalEncodingThreads")) {
		_master_encoding_threads = _server_encoding_threads = f.optional_number_child<int>("NumLocalEncodingThreads").get();
	} else {
		_master_encoding_threads = f.number_child<int>("MasterEncodingThreads");
		_server_encoding_threads = f.number_child<int>("ServerEncodingThreads");
	}

	_default_directory = f.optional_string_child ("DefaultDirectory");
	if (_default_directory && _default_directory->empty ()) {
		/* We used to store an empty value for this to mean "none set" */
		_default_directory = boost::optional<boost::filesystem::path> ();
	}

	boost::optional<int> b = f.optional_number_child<int> ("ServerPort");
	if (!b) {
		b = f.optional_number_child<int> ("ServerPortBase");
	}
	_server_port_base = b.get ();

	boost::optional<bool> u = f.optional_bool_child ("UseAnyServers");
	_use_any_servers = u.get_value_or (true);

	BOOST_FOREACH (cxml::ConstNodePtr i, f.node_children("Server")) {
		if (i->node_children("HostName").size() == 1) {
			_servers.push_back (i->string_child ("HostName"));
		} else {
			_servers.push_back (i->content ());
		}
	}

	_only_servers_encode = f.optional_bool_child ("OnlyServersEncode").get_value_or (false);
	_tms_protocol = static_cast<Protocol> (f.optional_number_child<int> ("TMSProtocol").get_value_or (static_cast<int> (PROTOCOL_SCP)));
	_tms_ip = f.string_child ("TMSIP");
	_tms_path = f.string_child ("TMSPath");
	_tms_user = f.string_child ("TMSUser");
	_tms_password = f.string_child ("TMSPassword");

	optional<string> c;
	c = f.optional_string_child ("SoundProcessor");
	if (c) {
		_cinema_sound_processor = CinemaSoundProcessor::from_id (c.get ());
	}
	c = f.optional_string_child ("CinemaSoundProcessor");
	if (c) {
		_cinema_sound_processor = CinemaSoundProcessor::from_id (c.get ());
	}

	_language = f.optional_string_child ("Language");

	c = f.optional_string_child ("DefaultContainer");
	if (c) {
		_default_container = Ratio::from_id (c.get ());
	}

	if (_default_container && !_default_container->used_for_container()) {
		Warning (_("Your default container is not valid and has been changed to Flat (1.85:1)"));
		_default_container = Ratio::from_id ("185");
	}

	c = f.optional_string_child ("DefaultScaleTo");
	if (c) {
		_default_scale_to = Ratio::from_id (c.get ());
	}

	c = f.optional_string_child ("DefaultDCPContentType");
	if (c) {
		_default_dcp_content_type = DCPContentType::from_isdcf_name (c.get ());
	}

	_default_dcp_audio_channels = f.optional_number_child<int>("DefaultDCPAudioChannels").get_value_or (6);

	if (f.optional_string_child ("DCPMetadataIssuer")) {
		_dcp_issuer = f.string_child ("DCPMetadataIssuer");
	} else if (f.optional_string_child ("DCPIssuer")) {
		_dcp_issuer = f.string_child ("DCPIssuer");
	}

	_default_upload_after_make_dcp = f.optional_bool_child("DefaultUploadAfterMakeDCP").get_value_or (false);
	_dcp_creator = f.optional_string_child ("DCPCreator").get_value_or ("");

	if (version && version.get() >= 2) {
		_default_isdcf_metadata = ISDCFMetadata (f.node_child ("ISDCFMetadata"));
	} else {
		_default_isdcf_metadata = ISDCFMetadata (f.node_child ("DCIMetadata"));
	}

	_default_still_length = f.optional_number_child<int>("DefaultStillLength").get_value_or (10);
	_default_j2k_bandwidth = f.optional_number_child<int>("DefaultJ2KBandwidth").get_value_or (200000000);
	_default_audio_delay = f.optional_number_child<int>("DefaultAudioDelay").get_value_or (0);
	_default_interop = f.optional_bool_child("DefaultInterop").get_value_or (false);
	_default_kdm_directory = f.optional_string_child("DefaultKDMDirectory");

	/* Load any cinemas from config.xml */
	read_cinemas (f);

	_mail_server = f.string_child ("MailServer");
	_mail_port = f.optional_number_child<int> ("MailPort").get_value_or (25);
	_mail_user = f.optional_string_child("MailUser").get_value_or ("");
	_mail_password = f.optional_string_child("MailPassword").get_value_or ("");
	_kdm_subject = f.optional_string_child ("KDMSubject").get_value_or (_("KDM delivery: $CPL_NAME"));
	_kdm_from = f.string_child ("KDMFrom");
	BOOST_FOREACH (cxml::ConstNodePtr i, f.node_children("KDMCC")) {
		if (!i->content().empty()) {
			_kdm_cc.push_back (i->content ());
		}
	}
	_kdm_bcc = f.optional_string_child ("KDMBCC").get_value_or ("");
	_kdm_email = f.string_child ("KDMEmail");

	_check_for_updates = f.optional_bool_child("CheckForUpdates").get_value_or (false);
	_check_for_test_updates = f.optional_bool_child("CheckForTestUpdates").get_value_or (false);

	_maximum_j2k_bandwidth = f.optional_number_child<int> ("MaximumJ2KBandwidth").get_value_or (250000000);
	_allow_any_dcp_frame_rate = f.optional_bool_child ("AllowAnyDCPFrameRate").get_value_or (false);

	_log_types = f.optional_number_child<int> ("LogTypes").get_value_or (LogEntry::TYPE_GENERAL | LogEntry::TYPE_WARNING | LogEntry::TYPE_ERROR);
	_analyse_ebur128 = f.optional_bool_child("AnalyseEBUR128").get_value_or (true);
	_automatic_audio_analysis = f.optional_bool_child ("AutomaticAudioAnalysis").get_value_or (false);
#ifdef DCPOMATIC_WINDOWS
	_win32_console = f.optional_bool_child ("Win32Console").get_value_or (false);
#endif

	list<cxml::NodePtr> his = f.node_children ("History");
	BOOST_FOREACH (cxml::ConstNodePtr i, f.node_children("History")) {
		_history.push_back (i->content ());
	}

	cxml::NodePtr signer = f.optional_node_child ("Signer");
	if (signer) {
		shared_ptr<dcp::CertificateChain> c (new dcp::CertificateChain ());
		/* Read the signing certificates and private key in from the config file */
		BOOST_FOREACH (cxml::NodePtr i, signer->node_children ("Certificate")) {
			c->add (dcp::Certificate (i->content ()));
		}
		c->set_key (signer->string_child ("PrivateKey"));
		_signer_chain = c;
	} else {
		/* Make a new set of signing certificates and key */
		_signer_chain = create_certificate_chain ();
	}

	cxml::NodePtr decryption = f.optional_node_child ("Decryption");
	if (decryption) {
		shared_ptr<dcp::CertificateChain> c (new dcp::CertificateChain ());
		BOOST_FOREACH (cxml::NodePtr i, decryption->node_children ("Certificate")) {
			c->add (dcp::Certificate (i->content ()));
		}
		c->set_key (decryption->string_child ("PrivateKey"));
		_decryption_chain = c;
	} else {
		_decryption_chain = create_certificate_chain ();
	}

	if (f.optional_node_child("DKDMGroup")) {
		/* New-style: all DKDMs in a group */
		_dkdms = dynamic_pointer_cast<DKDMGroup> (DKDMBase::read (f.node_child("DKDMGroup")));
	} else {
		/* Old-style: one or more DKDM nodes */
		_dkdms.reset (new DKDMGroup ("root"));
		BOOST_FOREACH (cxml::ConstNodePtr i, f.node_children("DKDM")) {
			_dkdms->add (DKDMBase::read (i));
		}
	}
	_cinemas_file = f.optional_string_child("CinemasFile").get_value_or (path ("cinemas.xml").string ());
	_show_hints_before_make_dcp = f.optional_bool_child("ShowHintsBeforeMakeDCP").get_value_or (true);
	_confirm_kdm_email = f.optional_bool_child("ConfirmKDMEmail").get_value_or (true);
	_kdm_container_name_format = dcp::NameFormat (f.optional_string_child("KDMContainerNameFormat").get_value_or ("KDM %f %c"));
	_kdm_filename_format = dcp::NameFormat (f.optional_string_child("KDMFilenameFormat").get_value_or ("KDM %f %c %s"));
	_dcp_metadata_filename_format = dcp::NameFormat (f.optional_string_child("DCPMetadataFilenameFormat").get_value_or ("%t"));
	_dcp_asset_filename_format = dcp::NameFormat (f.optional_string_child("DCPAssetFilenameFormat").get_value_or ("%t"));
	_jump_to_selected = f.optional_bool_child("JumpToSelected").get_value_or (true);
	BOOST_FOREACH (cxml::NodePtr i, f.node_children("Nagged")) {
		int const id = i->number_attribute<int>("Id");
		if (id >= 0 && id < NAG_COUNT) {
			_nagged[id] = raw_convert<int>(i->content());
		}
	}
	/* The variable was renamed but not the XML tag */
	_sound = f.optional_bool_child("PreviewSound").get_value_or (false);
	_sound_output = f.optional_string_child("PreviewSoundOutput");
	if (f.optional_string_child("CoverSheet")) {
		_cover_sheet = f.optional_string_child("CoverSheet").get();
	}
	_last_player_load_directory = f.optional_string_child("LastPlayerLoadDirectory");
	if (f.optional_string_child("LastKDMWriteType")) {
		if (f.optional_string_child("LastKDMWriteType").get() == "flat") {
			_last_kdm_write_type = KDM_WRITE_FLAT;
		} else if (f.optional_string_child("LastKDMWriteType").get() == "folder") {
			_last_kdm_write_type = KDM_WRITE_FOLDER;
		} else if (f.optional_string_child("LastKDMWriteType").get() == "zip") {
			_last_kdm_write_type = KDM_WRITE_ZIP;
		}
	}
	_frames_in_memory_multiplier = f.optional_number_child<int>("FramesInMemoryMultiplier").get_value_or(3);

	/* Replace any cinemas from config.xml with those from the configured file */
	if (boost::filesystem::exists (_cinemas_file)) {
		cxml::Document f ("Cinemas");
		f.read_file (_cinemas_file);
		read_cinemas (f);
	}
}
catch (...) {
	if (have_existing ("config.xml")) {

		/* Make a copy of the configuration */
		try {
			int n = 1;
			while (n < 100 && boost::filesystem::exists(path(String::compose("config.xml.%1", n)))) {
				++n;
			}

			boost::filesystem::copy_file(path("config.xml", false), path(String::compose("config.xml.%1", n), false));
			boost::filesystem::copy_file(path("cinemas.xml", false), path(String::compose("cinemas.xml.%1", n), false));
		} catch (...) {}

		/* We have a config file but it didn't load */
		FailedToLoad ();
	}
	set_defaults ();
	/* Make a new set of signing certificates and key */
	_signer_chain = create_certificate_chain ();
	/* And similar for decryption of KDMs */
	_decryption_chain = create_certificate_chain ();
	write ();
}

/** @return Filename to write configuration to */
boost::filesystem::path
Config::path (string file, bool create_directories)
{
	boost::filesystem::path p;
	if (test_path) {
		p = test_path.get();
	} else {
#ifdef DCPOMATIC_OSX
		p /= g_get_home_dir ();
		p /= "Library";
		p /= "Preferences";
		p /= "com.dcpomatic";
		p /= "2";
#else
		p /= g_get_user_config_dir ();
		p /= "dcpomatic2";
#endif
	}
	boost::system::error_code ec;
	if (create_directories) {
		boost::filesystem::create_directories (p, ec);
	}
	p /= file;
	return p;
}

/** @return Singleton instance */
Config *
Config::instance ()
{
	if (_instance == 0) {
		_instance = new Config;
		_instance->read ();
	}

	return _instance;
}

/** Write our configuration to disk */
void
Config::write () const
{
	write_config ();
	write_cinemas ();
}

void
Config::write_config () const
{
	xmlpp::Document doc;
	xmlpp::Element* root = doc.create_root_node ("Config");

	/* [XML] Version The version number of the configuration file format; currently 2. */
	root->add_child("Version")->add_child_text ("2");
	/* [XML] MasterEncodingThreads Number of encoding threads to use when running as master. */
	root->add_child("MasterEncodingThreads")->add_child_text (raw_convert<string> (_master_encoding_threads));
	/* [XML] ServerEncodingThreads Number of encoding threads to use when running as server. */
	root->add_child("ServerEncodingThreads")->add_child_text (raw_convert<string> (_server_encoding_threads));
	if (_default_directory) {
		/* [XML:opt] DefaultDirectory Default directory when creating a new film in the GUI. */
		root->add_child("DefaultDirectory")->add_child_text (_default_directory->string ());
	}
	/* [XML] ServerPortBase Port number to use for frame encoding requests.  <code>ServerPortBase</code> + 1 and
	   <code>ServerPortBase</code> + 2 are used for querying servers.  <code>ServerPortBase</code> + 3 is used
	   by the batch converter to listen for job requests.
	*/
	root->add_child("ServerPortBase")->add_child_text (raw_convert<string> (_server_port_base));
	/* [XML] UseAnyServers 1 to broadcast to look for encoding servers to use, 0 to use only those configured. */
	root->add_child("UseAnyServers")->add_child_text (_use_any_servers ? "1" : "0");

	BOOST_FOREACH (string i, _servers) {
		/* [XML:opt] Server IP address or hostname of an encoding server to use; you can use as many of these tags
		   as you like.
		*/
		root->add_child("Server")->add_child_text (i);
	}

	/* [XML] OnlyServersEncode 1 to set the master to do decoding of source content no JPEG2000 encoding; all encoding
	   is done by the encoding servers.  0 to set the master to do some encoding as well as coordinating the job.
	*/
	root->add_child("OnlyServersEncode")->add_child_text (_only_servers_encode ? "1" : "0");
	/* [XML] TMSProtocol Protocol to use to copy files to a TMS; 0 to use SCP, 1 for FTP. */
	root->add_child("TMSProtocol")->add_child_text (raw_convert<string> (static_cast<int> (_tms_protocol)));
	/* [XML] TMSIP IP address of TMS */
	root->add_child("TMSIP")->add_child_text (_tms_ip);
	/* [XML] TMSPath Path on the TMS to copy files to */
	root->add_child("TMSPath")->add_child_text (_tms_path);
	/* [XML] TMSUser Username to log into the TMS with */
	root->add_child("TMSUser")->add_child_text (_tms_user);
	/* [XML] TMSPassword Password to log into the TMS with */
	root->add_child("TMSPassword")->add_child_text (_tms_password);
	if (_cinema_sound_processor) {
		/* [XML:opt] CinemaSoundProcessor Identifier of the type of cinema sound processor to use when calculating
		   gain changes from fader positions.  Currently can only be <code>dolby_cp750</code>.
		*/
		root->add_child("CinemaSoundProcessor")->add_child_text (_cinema_sound_processor->id ());
	}
	if (_language) {
		/* [XML:opt] Language Language to use in the GUI e.g. <code>fr_FR</code>. */
		root->add_child("Language")->add_child_text (_language.get());
	}
	if (_default_container) {
		/* [XML:opt] DefaultContainer ID of default container
		 * to use when creating new films (<code>185</code>,<code>239</code> or
		 * <code>190</code>).
		*/
		root->add_child("DefaultContainer")->add_child_text (_default_container->id ());
	}
	if (_default_scale_to) {
		/* [XML:opt] DefaultScaleTo ID of default ratio to scale content to when creating new films
		   (see <code>DefaultContainer</code> for IDs).
		*/
		root->add_child("DefaultScaleTo")->add_child_text (_default_scale_to->id ());
	}
	if (_default_dcp_content_type) {
		/* [XML:opt] DefaultDCPContentType Default content type ot use when creating new films (<code>FTR</code>, <code>SHR</code>,
		   <code>TLR</code>, <code>TST</code>, <code>XSN</code>, <code>RTG</code>, <code>TSR</code>, <code>POL</code>,
		   <code>PSA</code> or <code>ADV</code>). */
		root->add_child("DefaultDCPContentType")->add_child_text (_default_dcp_content_type->isdcf_name ());
	}
	/* [XML] DefaultDCPAudioChannels Default number of audio channels to use when creating new films. */
	root->add_child("DefaultDCPAudioChannels")->add_child_text (raw_convert<string> (_default_dcp_audio_channels));
	/* [XML] DCPIssuer Issuer text to write into CPL files. */
	root->add_child("DCPIssuer")->add_child_text (_dcp_issuer);
	/* [XML] DCPIssuer Creator text to write into CPL files. */
	root->add_child("DCPCreator")->add_child_text (_dcp_creator);
	root->add_child("DefaultUploadAfterMakeDCP")->add_child_text (_default_upload_after_make_dcp ? "1" : "0");

	/* [XML] ISDCFMetadata Default ISDCF metadata to use for new films; child tags are <code>&lt;ContentVersion&gt;</code>,
	   <code>&lt;AudioLanguage&gt;</code>, <code>&lt;SubtitleLanguage&gt;</code>, <code>&lt;Territory&gt;</code>,
	   <code>&lt;Rating&gt;</code>, <code>&lt;Studio&gt;</code>, <code>&lt;Facility&gt;</code>, <code>&lt;TempVersion&gt;</code>,
	   <code>&lt;PreRelease&gt;</code>, <code>&lt;RedBand&gt;</code>, <code>&lt;Chain&gt;</code>, <code>&lt;TwoDVersionOFThreeD&gt;</code>,
	   <code>&lt;MasteredLuminance&gt;</code>.
	*/
	_default_isdcf_metadata.as_xml (root->add_child ("ISDCFMetadata"));

	/* [XML] DefaultStillLength Default length (in seconds) for still images in new films. */
	root->add_child("DefaultStillLength")->add_child_text (raw_convert<string> (_default_still_length));
	/* [XML] DefaultJ2KBandwidth Default bitrate (in bits per second) for JPEG2000 data in new films. */
	root->add_child("DefaultJ2KBandwidth")->add_child_text (raw_convert<string> (_default_j2k_bandwidth));
	/* [XML] DefaultAudioDelay Default delay to apply to audio (positive moves audio later) in milliseconds. */
	root->add_child("DefaultAudioDelay")->add_child_text (raw_convert<string> (_default_audio_delay));
	/* [XML] DefaultInterop 1 to default new films to Interop, 0 for SMPTE. */
	root->add_child("DefaultInterop")->add_child_text (_default_interop ? "1" : "0");
	if (_default_kdm_directory) {
		/* [XML:opt] DefaultKDMDirectory Default directory to write KDMs to. */
		root->add_child("DefaultKDMDirectory")->add_child_text (_default_kdm_directory->string ());
	}
	/* [XML] MailServer Hostname of SMTP server to use. */
	root->add_child("MailServer")->add_child_text (_mail_server);
	/* [XML] MailPort Port number to use on SMTP server. */
	root->add_child("MailPort")->add_child_text (raw_convert<string> (_mail_port));
	/* [XML] MailUser Username to use on SMTP server. */
	root->add_child("MailUser")->add_child_text (_mail_user);
	/* [XML] MailPassword Password to use on SMTP server. */
	root->add_child("MailPassword")->add_child_text (_mail_password);
	/* [XML] KDMSubject Subject to use for KDM emails. */
	root->add_child("KDMSubject")->add_child_text (_kdm_subject);
	/* [XML] KDMFrom From address to use for KDM emails. */
	root->add_child("KDMFrom")->add_child_text (_kdm_from);
	BOOST_FOREACH (string i, _kdm_cc) {
		/* [XML] KDMCC CC address to use for KDM emails; you can use as many of these tags as you like. */
		root->add_child("KDMCC")->add_child_text (i);
	}
	/* [XML] KDMBCC BCC address to use for KDM emails */
	root->add_child("KDMBCC")->add_child_text (_kdm_bcc);
	/* [XML] KDMEmail Text of KDM email */
	root->add_child("KDMEmail")->add_child_text (_kdm_email);

	/* [XML] CheckForUpdates 1 to check dcpomatic.com for new versions, 0 to check only on request */
	root->add_child("CheckForUpdates")->add_child_text (_check_for_updates ? "1" : "0");
	/* [XML] CheckForUpdates 1 to check dcpomatic.com for new text versions, 0 to check only on request */
	root->add_child("CheckForTestUpdates")->add_child_text (_check_for_test_updates ? "1" : "0");

	/* [XML] MaximumJ2KBandwidth Maximum J2K bandwidth (in bits per second) that can be specified in the GUI */
	root->add_child("MaximumJ2KBandwidth")->add_child_text (raw_convert<string> (_maximum_j2k_bandwidth));
	/* [XML] AllowAnyDCPFrameRate 1 to allow users to specify any frame rate when creating DCPs, 0 to limit the GUI to standard rates */
	root->add_child("AllowAnyDCPFrameRate")->add_child_text (_allow_any_dcp_frame_rate ? "1" : "0");
	/* [XML] LogTypes Types of logging to write; a bitfield where 1 is general notes, 2 warnings, 4 errors, 8 debug information related
	   to encoding, 16 debug information related to encoding, 32 debug information for timing purposes, 64 debug information related
	   to sending email.
	*/
	root->add_child("LogTypes")->add_child_text (raw_convert<string> (_log_types));
	/* [XML] AnalyseEBUR128 1 to do EBUR128 analyses when analysing audio, otherwise 0. */
	root->add_child("AnalyseEBUR128")->add_child_text (_analyse_ebur128 ? "1" : "0");
	/* [XML] AutomaticAudioAnalysis 1 to run audio analysis automatically when audio content is added to the film, otherwise 0. */
	root->add_child("AutomaticAudioAnalysis")->add_child_text (_automatic_audio_analysis ? "1" : "0");
#ifdef DCPOMATIC_WINDOWS
	/* [XML] Win32Console 1 to open a console when running on Windows, otherwise 0. */
	root->add_child("Win32Console")->add_child_text (_win32_console ? "1" : "0");
#endif

	/* [XML] Signer Certificate chain and private key to use when signing DCPs and KDMs.  Should contain <code>&lt;Certificate&gt;</code>
	   tags in order and a <code>&lt;PrivateKey&gt;</code> tag all containing PEM-encoded certificates or private keys as appropriate.
	*/
	xmlpp::Element* signer = root->add_child ("Signer");
	DCPOMATIC_ASSERT (_signer_chain);
	BOOST_FOREACH (dcp::Certificate const & i, _signer_chain->unordered()) {
		signer->add_child("Certificate")->add_child_text (i.certificate (true));
	}
	signer->add_child("PrivateKey")->add_child_text (_signer_chain->key().get ());

	/* [XML] Decryption Certificate chain and private key to use when decrypting KDMs */
	xmlpp::Element* decryption = root->add_child ("Decryption");
	DCPOMATIC_ASSERT (_decryption_chain);
	BOOST_FOREACH (dcp::Certificate const & i, _decryption_chain->unordered()) {
		decryption->add_child("Certificate")->add_child_text (i.certificate (true));
	}
	decryption->add_child("PrivateKey")->add_child_text (_decryption_chain->key().get ());

	/* [XML] History Filename of DCP to present in the <guilabel>File</guilabel> menu of the GUI; there can be more than one
	   of these tags.
	*/
	BOOST_FOREACH (boost::filesystem::path i, _history) {
		root->add_child("History")->add_child_text (i.string ());
	}

	/* [XML] DKDMGroup A group of DKDMs, each with a <code>Name</code> attribute, containing other <code>&lt;DKDMGroup&gt;</code>
	   or <code>&lt;DKDM&gt;</code> tags.
	*/
	/* [XML] DKDM A DKDM as XML */
	_dkdms->as_xml (root);

	/* [XML] CinemasFile Filename of cinemas list file */
	root->add_child("CinemasFile")->add_child_text (_cinemas_file.string());
	/* [XML] ShowHintsBeforeMakeDCP 1 to show hints in the GUI before making a DCP, otherwise 0 */
	root->add_child("ShowHintsBeforeMakeDCP")->add_child_text (_show_hints_before_make_dcp ? "1" : "0");
	/* [XML] ConfirmKDMEmail 1 to confirm before sending KDM emails in the GUI, otherwise 0 */
	root->add_child("ConfirmKDMEmail")->add_child_text (_confirm_kdm_email ? "1" : "0");
	/* [XML] KDMFilenameFormat Format for KDM filenames */
	root->add_child("KDMFilenameFormat")->add_child_text (_kdm_filename_format.specification ());
	/* [XML] KDMContainerNameFormat Format for KDM containers (directories or ZIP files) */
	root->add_child("KDMContainerNameFormat")->add_child_text (_kdm_container_name_format.specification ());
	/* [XML] DCPMetadataFilenameFormat Format for DCP metadata filenames */
	root->add_child("DCPMetadataFilenameFormat")->add_child_text (_dcp_metadata_filename_format.specification ());
	/* [XML] DCPAssetFilenameFormat Format for DCP asset filenames */
	root->add_child("DCPAssetFilenameFormat")->add_child_text (_dcp_asset_filename_format.specification ());
	/* [XML] JumpToSelected 1 to make the GUI jump to the start of content when it is selected, otherwise 0 */
	root->add_child("JumpToSelected")->add_child_text (_jump_to_selected ? "1" : "0");
	/* [XML] Nagged 1 if a particular nag screen has been shown and should not be shown again, otherwise 0 */
	for (int i = 0; i < NAG_COUNT; ++i) {
		xmlpp::Element* e = root->add_child ("Nagged");
		e->set_attribute ("Id", raw_convert<string>(i));
		e->add_child_text (_nagged[i] ? "1" : "0");
	}
	/* [XML] PreviewSound 1 to use sound in the GUI preview and player, otherwise 0 */
	root->add_child("PreviewSound")->add_child_text (_sound ? "1" : "0");
	if (_sound_output) {
		/* [XML:opt] PreviewSoundOutput Name of the audio output to use */
		root->add_child("PreviewSoundOutput")->add_child_text (_sound_output.get());
	}
	/* [XML] CoverSheet Text of the cover sheet to write when making DCPs */
	root->add_child("CoverSheet")->add_child_text (_cover_sheet);
	if (_last_player_load_directory) {
		root->add_child("LastPlayerLoadDirectory")->add_child_text(_last_player_load_directory->string());
	}
	if (_last_kdm_write_type) {
		switch (_last_kdm_write_type.get()) {
		case KDM_WRITE_FLAT:
			root->add_child("LastKDMWriteType")->add_child_text("flat");
			break;
		case KDM_WRITE_FOLDER:
			root->add_child("LastKDMWriteType")->add_child_text("folder");
			break;
		case KDM_WRITE_ZIP:
			root->add_child("LastKDMWriteType")->add_child_text("zip");
			break;
		}
	}
	/* [XML] FramesInMemoryMultiplier value to multiply the encoding threads count by to get the maximum number of
	   frames to be held in memory at once.
	*/
	root->add_child("FramesInMemoryMultiplier")->add_child_text(raw_convert<string>(_frames_in_memory_multiplier));

	try {
		doc.write_to_file_formatted(config_file().string());
	} catch (xmlpp::exception& e) {
		string s = e.what ();
		trim (s);
		throw FileError (s, path("config.xml"));
	}
}

void
Config::write_cinemas () const
{
	xmlpp::Document doc;
	xmlpp::Element* root = doc.create_root_node ("Cinemas");
	root->add_child("Version")->add_child_text ("1");

	BOOST_FOREACH (shared_ptr<Cinema> i, _cinemas) {
		i->as_xml (root->add_child ("Cinema"));
	}

	try {
		doc.write_to_file_formatted (_cinemas_file.string ());
	} catch (xmlpp::exception& e) {
		string s = e.what ();
		trim (s);
		throw FileError (s, _cinemas_file);
	}
}

boost::filesystem::path
Config::default_directory_or (boost::filesystem::path a) const
{
	return directory_or (_default_directory, a);
}

boost::filesystem::path
Config::default_kdm_directory_or (boost::filesystem::path a) const
{
	return directory_or (_default_kdm_directory, a);
}

boost::filesystem::path
Config::directory_or (optional<boost::filesystem::path> dir, boost::filesystem::path a) const
{
	if (!dir) {
		return a;
	}

	boost::system::error_code ec;
	bool const e = boost::filesystem::exists (*dir, ec);
	if (ec || !e) {
		return a;
	}

	return *dir;
}

void
Config::drop ()
{
	delete _instance;
	_instance = 0;
}

void
Config::changed (Property what)
{
	Changed (what);
}

void
Config::set_kdm_email_to_default ()
{
	_kdm_subject = _("KDM delivery: $CPL_NAME");

	_kdm_email = _(
		"Dear Projectionist\n\n"
		"Please find attached KDMs for $CPL_NAME.\n\n"
		"Cinema: $CINEMA_NAME\n"
		"Screen(s): $SCREENS\n\n"
		"The KDMs are valid from $START_TIME until $END_TIME.\n\n"
		"Best regards,\nDCP-o-matic"
		);
}

void
Config::reset_kdm_email ()
{
	set_kdm_email_to_default ();
	changed ();
}

void
Config::set_cover_sheet_to_default ()
{
	_cover_sheet = _(
		"$CPL_NAME\n\n"
		"Type: $TYPE\n"
		"Format: $CONTAINER\n"
		"Audio: $AUDIO\n"
		"Audio Language: $AUDIO_LANGUAGE\n"
		"Subtitle Language: $SUBTITLE_LANGUAGE\n"
		"Length: $LENGTH\n"
		"Size: $SIZE\n"
		);
}

void
Config::add_to_history (boost::filesystem::path p)
{
	/* Remove existing instances of this path in the history */
	_history.erase (remove (_history.begin(), _history.end(), p), _history.end ());

	_history.insert (_history.begin (), p);
	if (_history.size() > HISTORY_SIZE) {
		_history.pop_back ();
	}

	changed ();
}

bool
Config::have_existing (string file)
{
	return boost::filesystem::exists (path (file, false));
}

void
Config::read_cinemas (cxml::Document const & f)
{
	_cinemas.clear ();
	list<cxml::NodePtr> cin = f.node_children ("Cinema");
	BOOST_FOREACH (cxml::ConstNodePtr i, f.node_children("Cinema")) {
		/* Slightly grotty two-part construction of Cinema here so that we can use
		   shared_from_this.
		*/
		shared_ptr<Cinema> cinema (new Cinema (i));
		cinema->read_screens (i);
		_cinemas.push_back (cinema);
	}
}

void
Config::set_cinemas_file (boost::filesystem::path file)
{
	_cinemas_file = file;

	if (boost::filesystem::exists (_cinemas_file)) {
		/* Existing file; read it in */
		cxml::Document f ("Cinemas");
		f.read_file (_cinemas_file);
		read_cinemas (f);
	}

	changed (OTHER);
}

void
Config::save_template (shared_ptr<const Film> film, string name) const
{
	film->write_template (template_path (name));
}

list<string>
Config::templates () const
{
	if (!boost::filesystem::exists (path ("templates"))) {
		return list<string> ();
	}

	list<string> n;
	for (boost::filesystem::directory_iterator i (path("templates")); i != boost::filesystem::directory_iterator(); ++i) {
		n.push_back (i->path().filename().string());
	}
	return n;
}

bool
Config::existing_template (string name) const
{
	return boost::filesystem::exists (template_path (name));
}

boost::filesystem::path
Config::template_path (string name) const
{
	return path("templates") / tidy_for_filename (name);
}

void
Config::rename_template (string old_name, string new_name) const
{
	boost::filesystem::rename (template_path (old_name), template_path (new_name));
}

void
Config::delete_template (string name) const
{
	boost::filesystem::remove (template_path (name));
}

/** @return Path to the config.xml containing the actual settings, following a link if required */
boost::filesystem::path
Config::config_file ()
{
	cxml::Document f ("Config");
	boost::filesystem::path main = path("config.xml", false);
	if (!boost::filesystem::exists (main)) {
		/* It doesn't exist, so there can't be any links; just return it */
		return main;
	}

	/* See if there's a link */
	f.read_file (main);
	optional<string> link = f.optional_string_child("Link");
	if (link) {
		return *link;
	}

	return main;
}

void
Config::reset_cover_sheet ()
{
	set_cover_sheet_to_default ();
	changed ();
}

void
Config::link (boost::filesystem::path new_file) const
{
	xmlpp::Document doc;
	doc.create_root_node("Config")->add_child("Link")->add_child_text(new_file.string());
	try {
		doc.write_to_file_formatted(path("config.xml", true).string());
	} catch (xmlpp::exception& e) {
		string s = e.what ();
		trim (s);
		throw FileError (s, path("config.xml"));
	}
}

void
Config::copy_and_link (boost::filesystem::path new_file) const
{
	write ();
	boost::filesystem::copy_file (config_file(), new_file, boost::filesystem::copy_option::overwrite_if_exists);
	link (new_file);
}
