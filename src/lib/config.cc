/*
    Copyright (C) 2012-2022 Carl Hetherington <cth@carlh.net>

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


#include "cinema.h"
#include "colour_conversion.h"
#include "compose.hpp"
#include "config.h"
#include "constants.h"
#include "cross.h"
#include "dcp_content_type.h"
#include "dkdm_recipient.h"
#include "dkdm_wrapper.h"
#include "film.h"
#include "filter.h"
#include "log.h"
#include "ratio.h"
#include "zipper.h"
#include <dcp/certificate_chain.h>
#include <dcp/name_format.h>
#include <dcp/raw_convert.h>
#include <libcxml/cxml.h>
#include <glib.h>
#include <libxml++/libxml++.h>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/thread.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include "i18n.h"


using std::cout;
using std::dynamic_pointer_cast;
using std::ifstream;
using std::list;
using std::make_shared;
using std::max;
using std::min;
using std::remove;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::algorithm::trim;
using boost::optional;
using dcp::raw_convert;


Config* Config::_instance = 0;
int const Config::_current_version = 3;
boost::signals2::signal<void (Config::LoadFailure)> Config::FailedToLoad;
boost::signals2::signal<void (string)> Config::Warning;
boost::signals2::signal<bool (Config::BadReason)> Config::Bad;


/** Construct default configuration */
Config::Config ()
        /* DKDMs are not considered a thing to reset on set_defaults() */
	: _dkdms (new DKDMGroup ("root"))
	, _default_kdm_duration (1, RoughDuration::Unit::WEEKS)
	, _export(this)
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
	_tms_protocol = FileTransferProtocol::SCP;
	_tms_passive = true;
	_tms_ip = "";
	_tms_path = ".";
	_tms_user = "";
	_tms_password = "";
	_allow_any_dcp_frame_rate = false;
	_allow_any_container = false;
	_allow_96khz_audio = false;
	_use_all_audio_channels = false;
	_show_experimental_audio_processors = false;
	_language = optional<string> ();
	_default_still_length = 10;
	_default_dcp_content_type = DCPContentType::from_isdcf_name ("FTR");
	_default_dcp_audio_channels = 8;
	_default_j2k_bandwidth = 150000000;
	_default_audio_delay = 0;
	_default_interop = false;
	_default_metadata.clear ();
	_upload_after_make_dcp = false;
	_mail_server = "";
	_mail_port = 25;
	_mail_protocol = EmailProtocol::AUTO;
	_mail_user = "";
	_mail_password = "";
	_kdm_from = "";
	_kdm_cc.clear ();
	_kdm_bcc = "";
	_notification_from = "";
	_notification_to = "";
	_notification_cc.clear ();
	_notification_bcc = "";
	_check_for_updates = false;
	_check_for_test_updates = false;
	_maximum_j2k_bandwidth = 250000000;
	_log_types = LogEntry::TYPE_GENERAL | LogEntry::TYPE_WARNING | LogEntry::TYPE_ERROR | LogEntry::TYPE_DISK;
	_analyse_ebur128 = true;
	_automatic_audio_analysis = false;
#ifdef DCPOMATIC_WINDOWS
	_win32_console = false;
#endif
	/* At the moment we don't write these files anywhere new after a version change, so they will be read from
	 * ~/.config/dcpomatic2 (or equivalent) and written back there.
	 */
	_cinemas_file = read_path ("cinemas.xml");
	_dkdm_recipients_file = read_path ("dkdm_recipients.xml");
	_show_hints_before_make_dcp = true;
	_confirm_kdm_email = true;
	_kdm_container_name_format = dcp::NameFormat("KDM_%f_%c");
	_kdm_filename_format = dcp::NameFormat("KDM_%f_%c_%s");
	_dkdm_filename_format = dcp::NameFormat("DKDM_%f_%c_%s");
	_dcp_metadata_filename_format = dcp::NameFormat ("%t");
	_dcp_asset_filename_format = dcp::NameFormat ("%t");
	_jump_to_selected = true;
	for (int i = 0; i < NAG_COUNT; ++i) {
		_nagged[i] = false;
	}
	_sound = true;
	_sound_output = optional<string> ();
	_last_kdm_write_type = KDM_WRITE_FLAT;
	_last_dkdm_write_type = DKDM_WRITE_INTERNAL;
	_default_add_file_location = DefaultAddFileLocation::SAME_AS_LAST_TIME;

	/* I think the scaling factor here should be the ratio of the longest frame
	   encode time to the shortest; if the thread count is T, longest time is L
	   and the shortest time S we could encode L/S frames per thread whilst waiting
	   for the L frame to encode so we might have to store LT/S frames.

	   However we don't want to use too much memory, so keep it a bit lower than we'd
	   perhaps like.  A J2K frame is typically about 1Mb so 3 here will mean we could
	   use about 240Mb with 72 encoding threads.
	*/
	_frames_in_memory_multiplier = 3;
	_decode_reduction = optional<int>();
	_default_notify = false;
	for (int i = 0; i < NOTIFICATION_COUNT; ++i) {
		_notification[i] = false;
	}
	_barco_username = optional<string>();
	_barco_password = optional<string>();
	_christie_username = optional<string>();
	_christie_password = optional<string>();
	_gdc_username = optional<string>();
	_gdc_password = optional<string>();
	_player_mode = PLAYER_MODE_WINDOW;
	_image_display = 0;
	_video_view_type = VIDEO_VIEW_SIMPLE;
	_respect_kdm_validity_periods = true;
	_player_debug_log_file = boost::none;
	_player_content_directory = boost::none;
	_player_playlist_directory = boost::none;
	_player_kdm_directory = boost::none;
	_audio_mapping = boost::none;
	_custom_languages.clear ();
	_initial_paths.clear();
	_initial_paths["AddFilesPath"] = boost::none;
	_initial_paths["AddKDMPath"] = boost::none;
	_initial_paths["AddDKDMPath"] = boost::none;
	_initial_paths["SelectCertificatePath"] = boost::none;
	_initial_paths["AddCombinerInputPath"] = boost::none;
	_initial_paths["ExportSubtitlesPath"] = boost::none;
	_initial_paths["ExportVideoPath"] = boost::none;
	_initial_paths["DebugLogPath"] = boost::none;
	_initial_paths["CinemaDatabasePath"] = boost::none;
	_initial_paths["ConfigFilePath"] = boost::none;
	_use_isdcf_name_by_default = true;
	_write_kdms_to_disk = true;
	_email_kdms = false;
	_default_kdm_type = dcp::Formulation::MODIFIED_TRANSITIONAL_1;
	_default_kdm_duration = RoughDuration(1, RoughDuration::Unit::WEEKS);
	_auto_crop_threshold = 0.1;
	_last_release_notes_version = boost::none;
	_allow_smpte_bv20 = false;
	_isdcf_name_part_length = 14;

	_allowed_dcp_frame_rates.clear ();
	_allowed_dcp_frame_rates.push_back (24);
	_allowed_dcp_frame_rates.push_back (25);
	_allowed_dcp_frame_rates.push_back (30);
	_allowed_dcp_frame_rates.push_back (48);
	_allowed_dcp_frame_rates.push_back (50);
	_allowed_dcp_frame_rates.push_back (60);

	set_kdm_email_to_default ();
	set_notification_email_to_default ();
	set_cover_sheet_to_default ();

#ifdef DCPOMATIC_GROK
	_grok = boost::none;
#endif

	_main_divider_sash_position = {};
	_main_content_divider_sash_position = {};

	_export.set_defaults();
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
	return make_shared<dcp::CertificateChain> (
		openssl_path(),
		CERTIFICATE_VALIDITY_PERIOD,
		"dcpomatic.com",
		"dcpomatic.com",
		".dcpomatic.smpte-430-2.ROOT",
		".dcpomatic.smpte-430-2.INTERMEDIATE",
		"CS.dcpomatic.smpte-430-2.LEAF"
		);
}

void
Config::backup ()
{
	using namespace boost::filesystem;

	auto copy_adding_number = [](path const& path_to_copy) {

		auto add_number = [](path const& path, int number) {
			return String::compose("%1.%2", path, number);
		};

		int n = 1;
		while (n < 100 && exists(add_number(path_to_copy, n))) {
			++n;
		}
		boost::system::error_code ec;
		copy_file(path_to_copy, add_number(path_to_copy, n), ec);
	};

	/* Make a backup copy of any config.xml, cinemas.xml, dkdm_recipients.xml that we might be about
	 * to write over.  This is more intended for the situation where we have a corrupted config.xml,
	 * and decide to overwrite it with a new one (possibly losing important details in the corrupted
	 * file).  But we might as well back up the other files while we're about it.
	 */

	/* This uses the State::write_path stuff so, e.g. for a current version 2.16 we might copy
	 * ~/.config/dcpomatic2/2.16/config.xml to ~/.config/dcpomatic2/2.16/config.xml.1
	 */
	copy_adding_number (config_write_file());

	/* These do not use State::write_path, so whatever path is in the Config we will copy
	 * adding a number.
	 */
	copy_adding_number (_cinemas_file);
	copy_adding_number (_dkdm_recipients_file);
}

void
Config::read ()
{
	read_config();
	read_cinemas();
	read_dkdm_recipients();
}


void
Config::read_config()
try
{
	cxml::Document f ("Config");
	f.read_file(dcp::filesystem::fix_long_path(config_read_file()));

	auto version = f.optional_number_child<int> ("Version");
	if (version && *version < _current_version) {
		/* Back up the old config before we re-write it in a back-incompatible way */
		backup ();
	}

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

	auto b = f.optional_number_child<int> ("ServerPort");
	if (!b) {
		b = f.optional_number_child<int> ("ServerPortBase");
	}
	_server_port_base = b.get ();

	auto u = f.optional_bool_child ("UseAnyServers");
	_use_any_servers = u.get_value_or (true);

	for (auto i: f.node_children("Server")) {
		if (i->node_children("HostName").size() == 1) {
			_servers.push_back (i->string_child ("HostName"));
		} else {
			_servers.push_back (i->content ());
		}
	}

	_only_servers_encode = f.optional_bool_child ("OnlyServersEncode").get_value_or (false);
	_tms_protocol = static_cast<FileTransferProtocol>(f.optional_number_child<int>("TMSProtocol").get_value_or(static_cast<int>(FileTransferProtocol::SCP)));
	_tms_passive = f.optional_bool_child("TMSPassive").get_value_or(true);
	_tms_ip = f.string_child ("TMSIP");
	_tms_path = f.string_child ("TMSPath");
	_tms_user = f.string_child ("TMSUser");
	_tms_password = f.string_child ("TMSPassword");

	_language = f.optional_string_child ("Language");

	_default_dcp_content_type = DCPContentType::from_isdcf_name(f.optional_string_child("DefaultDCPContentType").get_value_or("FTR"));
	_default_dcp_audio_channels = f.optional_number_child<int>("DefaultDCPAudioChannels").get_value_or (6);

	if (f.optional_string_child ("DCPMetadataIssuer")) {
		_dcp_issuer = f.string_child ("DCPMetadataIssuer");
	} else if (f.optional_string_child ("DCPIssuer")) {
		_dcp_issuer = f.string_child ("DCPIssuer");
	}

	auto up = f.optional_bool_child("UploadAfterMakeDCP");
	if (!up) {
		up = f.optional_bool_child("DefaultUploadAfterMakeDCP");
	}
	_upload_after_make_dcp = up.get_value_or (false);
	_dcp_creator = f.optional_string_child ("DCPCreator").get_value_or ("");
	_dcp_company_name = f.optional_string_child("DCPCompanyName").get_value_or("");
	_dcp_product_name = f.optional_string_child("DCPProductName").get_value_or("");
	_dcp_product_version = f.optional_string_child("DCPProductVersion").get_value_or("");
	_dcp_j2k_comment = f.optional_string_child("DCPJ2KComment").get_value_or("");

	_default_still_length = f.optional_number_child<int>("DefaultStillLength").get_value_or (10);
	_default_j2k_bandwidth = f.optional_number_child<int>("DefaultJ2KBandwidth").get_value_or (200000000);
	_default_audio_delay = f.optional_number_child<int>("DefaultAudioDelay").get_value_or (0);
	_default_interop = f.optional_bool_child("DefaultInterop").get_value_or (false);

	try {
		auto al = f.optional_string_child("DefaultAudioLanguage");
		if (al) {
			_default_audio_language = dcp::LanguageTag(*al);
		}
	} catch (std::runtime_error&) {}

	try {
		auto te = f.optional_string_child("DefaultTerritory");
		if (te) {
			_default_territory = dcp::LanguageTag::RegionSubtag(*te);
		}
	} catch (std::runtime_error&) {}

	for (auto const& i: f.node_children("DefaultMetadata")) {
		_default_metadata[i->string_attribute("key")] = i->content();
	}

	_default_kdm_directory = f.optional_string_child("DefaultKDMDirectory");

	/* Read any cinemas that are still lying around in the config file
	 * from an old version.
	 */
	read_cinemas (f);

	_mail_server = f.string_child ("MailServer");
	_mail_port = f.optional_number_child<int> ("MailPort").get_value_or (25);

	{
		/* Make sure this matches the code in write_config */
		string const protocol = f.optional_string_child("MailProtocol").get_value_or("Auto");
		if (protocol == "Auto") {
			_mail_protocol = EmailProtocol::AUTO;
		} else if (protocol == "Plain") {
			_mail_protocol = EmailProtocol::PLAIN;
		} else if (protocol == "STARTTLS") {
			_mail_protocol = EmailProtocol::STARTTLS;
		} else if (protocol == "SSL") {
			_mail_protocol = EmailProtocol::SSL;
		}
	}

	_mail_user = f.optional_string_child("MailUser").get_value_or ("");
	_mail_password = f.optional_string_child("MailPassword").get_value_or ("");

	_kdm_subject = f.optional_string_child ("KDMSubject").get_value_or (_("KDM delivery: $CPL_NAME"));
	_kdm_from = f.string_child ("KDMFrom");
	for (auto i: f.node_children("KDMCC")) {
		if (!i->content().empty()) {
			_kdm_cc.push_back (i->content ());
		}
	}
	_kdm_bcc = f.optional_string_child ("KDMBCC").get_value_or ("");
	_kdm_email = f.string_child ("KDMEmail");

	_notification_subject = f.optional_string_child("NotificationSubject").get_value_or(_("DCP-o-matic notification"));
	_notification_from = f.optional_string_child("NotificationFrom").get_value_or("");
	_notification_to = f.optional_string_child("NotificationTo").get_value_or("");
	for (auto i: f.node_children("NotificationCC")) {
		if (!i->content().empty()) {
			_notification_cc.push_back (i->content ());
		}
	}
	_notification_bcc = f.optional_string_child("NotificationBCC").get_value_or("");
	if (f.optional_string_child("NotificationEmail")) {
		_notification_email = f.string_child("NotificationEmail");
	}

	_check_for_updates = f.optional_bool_child("CheckForUpdates").get_value_or (false);
	_check_for_test_updates = f.optional_bool_child("CheckForTestUpdates").get_value_or (false);

	_maximum_j2k_bandwidth = f.optional_number_child<int> ("MaximumJ2KBandwidth").get_value_or (250000000);
	_allow_any_dcp_frame_rate = f.optional_bool_child ("AllowAnyDCPFrameRate").get_value_or (false);
	_allow_any_container = f.optional_bool_child ("AllowAnyContainer").get_value_or (false);
	_allow_96khz_audio = f.optional_bool_child("Allow96kHzAudio").get_value_or(false);
	_use_all_audio_channels = f.optional_bool_child("UseAllAudioChannels").get_value_or(false);
	_show_experimental_audio_processors = f.optional_bool_child ("ShowExperimentalAudioProcessors").get_value_or (false);

	_log_types = f.optional_number_child<int> ("LogTypes").get_value_or (LogEntry::TYPE_GENERAL | LogEntry::TYPE_WARNING | LogEntry::TYPE_ERROR);
	_analyse_ebur128 = f.optional_bool_child("AnalyseEBUR128").get_value_or (true);
	_automatic_audio_analysis = f.optional_bool_child ("AutomaticAudioAnalysis").get_value_or (false);
#ifdef DCPOMATIC_WINDOWS
	_win32_console = f.optional_bool_child ("Win32Console").get_value_or (false);
#endif

	for (auto i: f.node_children("History")) {
		_history.push_back (i->content ());
	}

	for (auto i: f.node_children("PlayerHistory")) {
		_player_history.push_back (i->content ());
	}

	auto signer = f.optional_node_child ("Signer");
	if (signer) {
		auto c = make_shared<dcp::CertificateChain>();
		/* Read the signing certificates and private key in from the config file */
		for (auto i: signer->node_children ("Certificate")) {
			c->add (dcp::Certificate (i->content ()));
		}
		c->set_key (signer->string_child ("PrivateKey"));
		_signer_chain = c;
	} else {
		/* Make a new set of signing certificates and key */
		_signer_chain = create_certificate_chain ();
	}

	auto decryption = f.optional_node_child ("Decryption");
	if (decryption) {
		auto c = make_shared<dcp::CertificateChain>();
		for (auto i: decryption->node_children ("Certificate")) {
			c->add (dcp::Certificate (i->content ()));
		}
		c->set_key (decryption->string_child ("PrivateKey"));
		_decryption_chain = c;
	} else {
		_decryption_chain = create_certificate_chain ();
	}

	/* These must be done before we call Bad as that might set one
	   of the nags.
	*/
	for (auto i: f.node_children("Nagged")) {
		auto const id = number_attribute<int>(i, "Id", "id");
		if (id >= 0 && id < NAG_COUNT) {
			_nagged[id] = raw_convert<int>(i->content());
		}
	}

	auto bad = check_certificates ();
	if (bad) {
		auto const remake = Bad(*bad);
		if (remake && *remake) {
			switch (*bad) {
			case BAD_SIGNER_UTF8_STRINGS:
			case BAD_SIGNER_INCONSISTENT:
			case BAD_SIGNER_VALIDITY_TOO_LONG:
			case BAD_SIGNER_DN_QUALIFIER:
				_signer_chain = create_certificate_chain ();
				break;
			case BAD_DECRYPTION_INCONSISTENT:
				_decryption_chain = create_certificate_chain ();
				break;
			}
		}
	}

	if (f.optional_node_child("DKDMGroup")) {
		/* New-style: all DKDMs in a group */
		_dkdms = dynamic_pointer_cast<DKDMGroup> (DKDMBase::read (f.node_child("DKDMGroup")));
	} else {
		/* Old-style: one or more DKDM nodes */
		_dkdms = make_shared<DKDMGroup>("root");
		for (auto i: f.node_children("DKDM")) {
			_dkdms->add (DKDMBase::read (i));
		}
	}
	_cinemas_file = f.optional_string_child("CinemasFile").get_value_or(read_path("cinemas.xml").string());
	_dkdm_recipients_file = f.optional_string_child("DKDMRecipientsFile").get_value_or(read_path("dkdm_recipients.xml").string());
	_show_hints_before_make_dcp = f.optional_bool_child("ShowHintsBeforeMakeDCP").get_value_or (true);
	_confirm_kdm_email = f.optional_bool_child("ConfirmKDMEmail").get_value_or (true);
	_kdm_container_name_format = dcp::NameFormat (f.optional_string_child("KDMContainerNameFormat").get_value_or ("KDM %f %c"));
	_kdm_filename_format = dcp::NameFormat (f.optional_string_child("KDMFilenameFormat").get_value_or ("KDM %f %c %s"));
	_dkdm_filename_format = dcp::NameFormat (f.optional_string_child("DKDMFilenameFormat").get_value_or("DKDM %f %c %s"));
	_dcp_metadata_filename_format = dcp::NameFormat (f.optional_string_child("DCPMetadataFilenameFormat").get_value_or ("%t"));
	_dcp_asset_filename_format = dcp::NameFormat (f.optional_string_child("DCPAssetFilenameFormat").get_value_or ("%t"));
	_jump_to_selected = f.optional_bool_child("JumpToSelected").get_value_or (true);
	/* The variable was renamed but not the XML tag */
	_sound = f.optional_bool_child("PreviewSound").get_value_or (true);
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
	if (f.optional_string_child("LastDKDMWriteType")) {
		if (f.optional_string_child("LastDKDMWriteType").get() == "internal") {
			_last_dkdm_write_type = DKDM_WRITE_INTERNAL;
		} else if (f.optional_string_child("LastDKDMWriteType").get() == "file") {
			_last_dkdm_write_type = DKDM_WRITE_FILE;
		}
	}
	_frames_in_memory_multiplier = f.optional_number_child<int>("FramesInMemoryMultiplier").get_value_or(3);
	_decode_reduction = f.optional_number_child<int>("DecodeReduction");
	_default_notify = f.optional_bool_child("DefaultNotify").get_value_or(false);

	for (auto i: f.node_children("Notification")) {
		int const id = number_attribute<int>(i, "Id", "id");
		if (id >= 0 && id < NOTIFICATION_COUNT) {
			_notification[id] = raw_convert<int>(i->content());
		}
	}

	_barco_username = f.optional_string_child("BarcoUsername");
	_barco_password = f.optional_string_child("BarcoPassword");
	_christie_username = f.optional_string_child("ChristieUsername");
	_christie_password = f.optional_string_child("ChristiePassword");
	_gdc_username = f.optional_string_child("GDCUsername");
	_gdc_password = f.optional_string_child("GDCPassword");

	auto pm = f.optional_string_child("PlayerMode");
	if (pm && *pm == "window") {
		_player_mode = PLAYER_MODE_WINDOW;
	} else if (pm && *pm == "full") {
		_player_mode = PLAYER_MODE_FULL;
	} else if (pm && *pm == "dual") {
		_player_mode = PLAYER_MODE_DUAL;
	}

	_image_display = f.optional_number_child<int>("ImageDisplay").get_value_or(0);
	auto vc = f.optional_string_child("VideoViewType");
	if (vc && *vc == "opengl") {
		_video_view_type = VIDEO_VIEW_OPENGL;
	} else if (vc && *vc == "simple") {
		_video_view_type = VIDEO_VIEW_SIMPLE;
	}
	_respect_kdm_validity_periods = f.optional_bool_child("RespectKDMValidityPeriods").get_value_or(true);
	_player_debug_log_file = f.optional_string_child("PlayerDebugLogFile");
	_player_content_directory = f.optional_string_child("PlayerContentDirectory");
	_player_playlist_directory = f.optional_string_child("PlayerPlaylistDirectory");
	_player_kdm_directory = f.optional_string_child("PlayerKDMDirectory");

	if (f.optional_node_child("AudioMapping")) {
		_audio_mapping = AudioMapping (f.node_child("AudioMapping"), Film::current_state_version);
	}

	for (auto i: f.node_children("CustomLanguage")) {
		try {
			/* This will fail if it's called before dcp::init() as it won't recognise the
			 * tag.  That's OK because the Config will be reloaded again later.
			 */
			_custom_languages.push_back (dcp::LanguageTag(i->content()));
		} catch (std::runtime_error& e) {}
	}

	for (auto& initial: _initial_paths) {
		initial.second = f.optional_string_child(initial.first);
	}
	_use_isdcf_name_by_default = f.optional_bool_child("UseISDCFNameByDefault").get_value_or(true);
	_write_kdms_to_disk = f.optional_bool_child("WriteKDMsToDisk").get_value_or(true);
	_email_kdms = f.optional_bool_child("EmailKDMs").get_value_or(false);
	_default_kdm_type = dcp::string_to_formulation(f.optional_string_child("DefaultKDMType").get_value_or("modified-transitional-1"));
	if (auto duration = f.optional_node_child("DefaultKDMDuration")) {
		_default_kdm_duration = RoughDuration(duration);
	} else {
		_default_kdm_duration = RoughDuration(1, RoughDuration::Unit::WEEKS);
	}
	_auto_crop_threshold = f.optional_number_child<double>("AutoCropThreshold").get_value_or(0.1);
	_last_release_notes_version = f.optional_string_child("LastReleaseNotesVersion");
	_main_divider_sash_position = f.optional_number_child<int>("MainDividerSashPosition");
	_main_content_divider_sash_position = f.optional_number_child<int>("MainContentDividerSashPosition");

	if (auto loc = f.optional_string_child("DefaultAddFileLocation")) {
		if (*loc == "last") {
			_default_add_file_location = DefaultAddFileLocation::SAME_AS_LAST_TIME;
		} else if (*loc == "project") {
			_default_add_file_location = DefaultAddFileLocation::SAME_AS_PROJECT;
		}
	}

	_allow_smpte_bv20 = f.optional_bool_child("AllowSMPTEBv20").get_value_or(false);
	_isdcf_name_part_length = f.optional_number_child<int>("ISDCFNamePartLength").get_value_or(14);

#ifdef DCPOMATIC_GROK
	if (auto grok = f.optional_node_child("Grok")) {
		_grok = Grok(grok);
	}
#endif

	_export.read(f.optional_node_child("Export"));
}
catch (...) {
	if (have_existing("config.xml")) {
		backup ();
		/* We have a config file but it didn't load */
		FailedToLoad(LoadFailure::CONFIG);
	}
	set_defaults ();
	/* Make a new set of signing certificates and key */
	_signer_chain = create_certificate_chain ();
	/* And similar for decryption of KDMs */
	_decryption_chain = create_certificate_chain ();
	write_config();
}


void
Config::read_cinemas()
{
	if (dcp::filesystem::exists(_cinemas_file)) {
		try {
			cxml::Document f("Cinemas");
			f.read_file(dcp::filesystem::fix_long_path(_cinemas_file));
			read_cinemas(f);
		} catch (...) {
			backup();
			FailedToLoad(LoadFailure::CINEMAS);
			write_cinemas();
		}
	}
}


void
Config::read_dkdm_recipients()
{
	if (dcp::filesystem::exists(_dkdm_recipients_file)) {
		try {
			cxml::Document f("DKDMRecipients");
			f.read_file(dcp::filesystem::fix_long_path(_dkdm_recipients_file));
			read_dkdm_recipients(f);
		} catch (...) {
			backup();
			FailedToLoad(LoadFailure::DKDM_RECIPIENTS);
			write_dkdm_recipients();
		}
	}
}


/** @return Singleton instance */
Config *
Config::instance ()
{
	if (_instance == nullptr) {
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
	write_dkdm_recipients ();
}

void
Config::write_config () const
{
	xmlpp::Document doc;
	auto root = doc.create_root_node ("Config");

	/* [XML] Version The version number of the configuration file format. */
	root->add_child("Version")->add_child_text (raw_convert<string>(_current_version));
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

	for (auto i: _servers) {
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
	/* [XML] TMSPassive True to use PASV mode with TMS FTP connections. */
	root->add_child("TMSPassive")->add_child_text(_tms_passive ? "1" : "0");
	/* [XML] TMSIP IP address of TMS. */
	root->add_child("TMSIP")->add_child_text (_tms_ip);
	/* [XML] TMSPath Path on the TMS to copy files to. */
	root->add_child("TMSPath")->add_child_text (_tms_path);
	/* [XML] TMSUser Username to log into the TMS with. */
	root->add_child("TMSUser")->add_child_text (_tms_user);
	/* [XML] TMSPassword Password to log into the TMS with. */
	root->add_child("TMSPassword")->add_child_text (_tms_password);
	if (_language) {
		/* [XML:opt] Language Language to use in the GUI e.g. <code>fr_FR</code>. */
		root->add_child("Language")->add_child_text (_language.get());
	}
	if (_default_dcp_content_type) {
		/* [XML:opt] DefaultDCPContentType Default content type to use when creating new films (<code>FTR</code>, <code>SHR</code>,
		   <code>TLR</code>, <code>TST</code>, <code>XSN</code>, <code>RTG</code>, <code>TSR</code>, <code>POL</code>,
		   <code>PSA</code> or <code>ADV</code>). */
		root->add_child("DefaultDCPContentType")->add_child_text (_default_dcp_content_type->isdcf_name ());
	}
	/* [XML] DefaultDCPAudioChannels Default number of audio channels to use when creating new films. */
	root->add_child("DefaultDCPAudioChannels")->add_child_text (raw_convert<string> (_default_dcp_audio_channels));
	/* [XML] DCPIssuer Issuer text to write into CPL files. */
	root->add_child("DCPIssuer")->add_child_text (_dcp_issuer);
	/* [XML] DCPCreator Creator text to write into CPL files. */
	root->add_child("DCPCreator")->add_child_text (_dcp_creator);
	/* [XML] Company name to write into MXF files. */
	root->add_child("DCPCompanyName")->add_child_text (_dcp_company_name);
	/* [XML] Product name to write into MXF files. */
	root->add_child("DCPProductName")->add_child_text (_dcp_product_name);
	/* [XML] Product version to write into MXF files. */
	root->add_child("DCPProductVersion")->add_child_text (_dcp_product_version);
	/* [XML] Comment to write into JPEG2000 data. */
	root->add_child("DCPJ2KComment")->add_child_text (_dcp_j2k_comment);
	/* [XML] UploadAfterMakeDCP 1 to upload to a TMS after making a DCP, 0 for no upload. */
	root->add_child("UploadAfterMakeDCP")->add_child_text (_upload_after_make_dcp ? "1" : "0");

	/* [XML] DefaultStillLength Default length (in seconds) for still images in new films. */
	root->add_child("DefaultStillLength")->add_child_text (raw_convert<string> (_default_still_length));
	/* [XML] DefaultJ2KBandwidth Default bitrate (in bits per second) for JPEG2000 data in new films. */
	root->add_child("DefaultJ2KBandwidth")->add_child_text (raw_convert<string> (_default_j2k_bandwidth));
	/* [XML] DefaultAudioDelay Default delay to apply to audio (positive moves audio later) in milliseconds. */
	root->add_child("DefaultAudioDelay")->add_child_text (raw_convert<string> (_default_audio_delay));
	/* [XML] DefaultInterop 1 to default new films to Interop, 0 for SMPTE. */
	root->add_child("DefaultInterop")->add_child_text (_default_interop ? "1" : "0");
	if (_default_audio_language) {
		/* [XML] DefaultAudioLanguage Default audio language to use for new films */
		root->add_child("DefaultAudioLanguage")->add_child_text(_default_audio_language->to_string());
	}
	if (_default_territory) {
		/* [XML] DefaultTerritory Default territory to use for new films */
		root->add_child("DefaultTerritory")->add_child_text(_default_territory->subtag());
	}
	for (auto const& i: _default_metadata) {
		auto c = root->add_child("DefaultMetadata");
		c->set_attribute("key", i.first);
		c->add_child_text(i.second);
	}
	if (_default_kdm_directory) {
		/* [XML:opt] DefaultKDMDirectory Default directory to write KDMs to. */
		root->add_child("DefaultKDMDirectory")->add_child_text (_default_kdm_directory->string ());
	}
	_default_kdm_duration.as_xml(root->add_child("DefaultKDMDuration"));
	/* [XML] MailServer Hostname of SMTP server to use. */
	root->add_child("MailServer")->add_child_text (_mail_server);
	/* [XML] MailPort Port number to use on SMTP server. */
	root->add_child("MailPort")->add_child_text (raw_convert<string> (_mail_port));
	/* [XML] MailProtocol Protocol to use on SMTP server (Auto, Plain, STARTTLS or SSL) */
	switch (_mail_protocol) {
	case EmailProtocol::AUTO:
		root->add_child("MailProtocol")->add_child_text("Auto");
		break;
	case EmailProtocol::PLAIN:
		root->add_child("MailProtocol")->add_child_text("Plain");
		break;
	case EmailProtocol::STARTTLS:
		root->add_child("MailProtocol")->add_child_text("STARTTLS");
		break;
	case EmailProtocol::SSL:
		root->add_child("MailProtocol")->add_child_text("SSL");
		break;
	}
	/* [XML] MailUser Username to use on SMTP server. */
	root->add_child("MailUser")->add_child_text (_mail_user);
	/* [XML] MailPassword Password to use on SMTP server. */
	root->add_child("MailPassword")->add_child_text (_mail_password);

	/* [XML] KDMSubject Subject to use for KDM emails. */
	root->add_child("KDMSubject")->add_child_text (_kdm_subject);
	/* [XML] KDMFrom From address to use for KDM emails. */
	root->add_child("KDMFrom")->add_child_text (_kdm_from);
	for (auto i: _kdm_cc) {
		/* [XML] KDMCC CC address to use for KDM emails; you can use as many of these tags as you like. */
		root->add_child("KDMCC")->add_child_text (i);
	}
	/* [XML] KDMBCC BCC address to use for KDM emails. */
	root->add_child("KDMBCC")->add_child_text (_kdm_bcc);
	/* [XML] KDMEmail Text of KDM email. */
	root->add_child("KDMEmail")->add_child_text (_kdm_email);

	/* [XML] NotificationSubject Subject to use for notification emails. */
	root->add_child("NotificationSubject")->add_child_text (_notification_subject);
	/* [XML] NotificationFrom From address to use for notification emails. */
	root->add_child("NotificationFrom")->add_child_text (_notification_from);
	/* [XML] NotificationFrom To address to use for notification emails. */
	root->add_child("NotificationTo")->add_child_text (_notification_to);
	for (auto i: _notification_cc) {
		/* [XML] NotificationCC CC address to use for notification emails; you can use as many of these tags as you like. */
		root->add_child("NotificationCC")->add_child_text (i);
	}
	/* [XML] NotificationBCC BCC address to use for notification emails. */
	root->add_child("NotificationBCC")->add_child_text (_notification_bcc);
	/* [XML] NotificationEmail Text of notification email. */
	root->add_child("NotificationEmail")->add_child_text (_notification_email);

	/* [XML] CheckForUpdates 1 to check dcpomatic.com for new versions, 0 to check only on request. */
	root->add_child("CheckForUpdates")->add_child_text (_check_for_updates ? "1" : "0");
	/* [XML] CheckForUpdates 1 to check dcpomatic.com for new text versions, 0 to check only on request. */
	root->add_child("CheckForTestUpdates")->add_child_text (_check_for_test_updates ? "1" : "0");

	/* [XML] MaximumJ2KBandwidth Maximum J2K bandwidth (in bits per second) that can be specified in the GUI. */
	root->add_child("MaximumJ2KBandwidth")->add_child_text (raw_convert<string> (_maximum_j2k_bandwidth));
	/* [XML] AllowAnyDCPFrameRate 1 to allow users to specify any frame rate when creating DCPs, 0 to limit the GUI to standard rates. */
	root->add_child("AllowAnyDCPFrameRate")->add_child_text (_allow_any_dcp_frame_rate ? "1" : "0");
	/* [XML] AllowAnyContainer 1 to allow users to user any container ratio for their DCP, 0 to limit the GUI to DCI Flat/Scope */
	root->add_child("AllowAnyContainer")->add_child_text (_allow_any_container ? "1" : "0");
	/* [XML] Allow96kHzAudio 1 to allow users to make DCPs with 96kHz audio, 0 to always make 48kHz DCPs */
	root->add_child("Allow96kHzAudio")->add_child_text(_allow_96khz_audio ? "1" : "0");
	/* [XML] UseAllAudioChannels 1 to allow users to map audio to all 16 DCP channels, 0 to limit to the channels used in standard DCPs */
	root->add_child("UseAllAudioChannels")->add_child_text(_use_all_audio_channels ? "1" : "0");
	/* [XML] ShowExperimentalAudioProcessors 1 to offer users the (experimental) audio upmixer processors, 0 to hide them */
	root->add_child("ShowExperimentalAudioProcessors")->add_child_text (_show_experimental_audio_processors ? "1" : "0");
	/* [XML] LogTypes Types of logging to write; a bitfield where 1 is general notes, 2 warnings, 4 errors, 8 debug information related
	   to 3D, 16 debug information related to encoding, 32 debug information for timing purposes, 64 debug information related
	   to sending email, 128 debug information related to the video view, 256 information about disk writing, 512 debug information
	   related to the player, 1024 debug information related to audio analyses.
	*/
	root->add_child("LogTypes")->add_child_text (raw_convert<string> (_log_types));
	/* [XML] AnalyseEBUR128 1 to do EBUR128 analyses when analysing audio, otherwise 0. */
	root->add_child("AnalyseEBUR128")->add_child_text (_analyse_ebur128 ? "1" : "0");
	/* [XML] AutomaticAudioAnalysis 1 to run audio analysis automatically when audio content is added to the film, otherwise 0. */
	root->add_child("AutomaticAudioAnalysis")->add_child_text (_automatic_audio_analysis ? "1" : "0");
#ifdef DCPOMATIC_WINDOWS
	if (_win32_console) {
		/* [XML] Win32Console 1 to open a console when running on Windows, otherwise 0.
		 * We only write this if it's true, which is a bit of a hack to allow unit tests to work
		 * more easily on Windows (without a platform-specific reference in config_write_utf8_test)
		 */
		root->add_child("Win32Console")->add_child_text ("1");
	}
#endif

	/* [XML] Signer Certificate chain and private key to use when signing DCPs and KDMs.  Should contain <code>&lt;Certificate&gt;</code>
	   tags in order and a <code>&lt;PrivateKey&gt;</code> tag all containing PEM-encoded certificates or private keys as appropriate.
	*/
	auto signer = root->add_child ("Signer");
	DCPOMATIC_ASSERT (_signer_chain);
	for (auto const& i: _signer_chain->unordered()) {
		signer->add_child("Certificate")->add_child_text (i.certificate (true));
	}
	signer->add_child("PrivateKey")->add_child_text (_signer_chain->key().get ());

	/* [XML] Decryption Certificate chain and private key to use when decrypting KDMs */
	auto decryption = root->add_child ("Decryption");
	DCPOMATIC_ASSERT (_decryption_chain);
	for (auto const& i: _decryption_chain->unordered()) {
		decryption->add_child("Certificate")->add_child_text (i.certificate (true));
	}
	decryption->add_child("PrivateKey")->add_child_text (_decryption_chain->key().get ());

	/* [XML] History Filename of DCP to present in the <guilabel>File</guilabel> menu of the GUI; there can be more than one
	   of these tags.
	*/
	for (auto i: _history) {
		root->add_child("History")->add_child_text (i.string ());
	}

	/* [XML] History Filename of DCP to present in the <guilabel>File</guilabel> menu of the player; there can be more than one
	   of these tags.
	*/
	for (auto i: _player_history) {
		root->add_child("PlayerHistory")->add_child_text (i.string ());
	}

	/* [XML] DKDMGroup A group of DKDMs, each with a <code>Name</code> attribute, containing other <code>&lt;DKDMGroup&gt;</code>
	   or <code>&lt;DKDM&gt;</code> tags.
	*/
	/* [XML] DKDM A DKDM as XML */
	_dkdms->as_xml (root);

	/* [XML] CinemasFile Filename of cinemas list file. */
	root->add_child("CinemasFile")->add_child_text (_cinemas_file.string());
	/* [XML] DKDMRecipientsFile Filename of DKDM recipients list file. */
	root->add_child("DKDMRecipientsFile")->add_child_text (_dkdm_recipients_file.string());
	/* [XML] ShowHintsBeforeMakeDCP 1 to show hints in the GUI before making a DCP, otherwise 0. */
	root->add_child("ShowHintsBeforeMakeDCP")->add_child_text (_show_hints_before_make_dcp ? "1" : "0");
	/* [XML] ConfirmKDMEmail 1 to confirm before sending KDM emails in the GUI, otherwise 0. */
	root->add_child("ConfirmKDMEmail")->add_child_text (_confirm_kdm_email ? "1" : "0");
	/* [XML] KDMFilenameFormat Format for KDM filenames. */
	root->add_child("KDMFilenameFormat")->add_child_text (_kdm_filename_format.specification ());
	/* [XML] KDMFilenameFormat Format for DKDM filenames. */
	root->add_child("DKDMFilenameFormat")->add_child_text(_dkdm_filename_format.specification());
	/* [XML] KDMContainerNameFormat Format for KDM containers (directories or ZIP files). */
	root->add_child("KDMContainerNameFormat")->add_child_text (_kdm_container_name_format.specification ());
	/* [XML] DCPMetadataFilenameFormat Format for DCP metadata filenames. */
	root->add_child("DCPMetadataFilenameFormat")->add_child_text (_dcp_metadata_filename_format.specification ());
	/* [XML] DCPAssetFilenameFormat Format for DCP asset filenames. */
	root->add_child("DCPAssetFilenameFormat")->add_child_text (_dcp_asset_filename_format.specification ());
	/* [XML] JumpToSelected 1 to make the GUI jump to the start of content when it is selected, otherwise 0. */
	root->add_child("JumpToSelected")->add_child_text (_jump_to_selected ? "1" : "0");
	/* [XML] Nagged 1 if a particular nag screen has been shown and should not be shown again, otherwise 0. */
	for (int i = 0; i < NAG_COUNT; ++i) {
		xmlpp::Element* e = root->add_child ("Nagged");
		e->set_attribute("id", raw_convert<string>(i));
		e->add_child_text (_nagged[i] ? "1" : "0");
	}
	/* [XML] PreviewSound 1 to use sound in the GUI preview and player, otherwise 0. */
	root->add_child("PreviewSound")->add_child_text (_sound ? "1" : "0");
	if (_sound_output) {
		/* [XML:opt] PreviewSoundOutput Name of the audio output to use. */
		root->add_child("PreviewSoundOutput")->add_child_text (_sound_output.get());
	}
	/* [XML] CoverSheet Text of the cover sheet to write when making DCPs. */
	root->add_child("CoverSheet")->add_child_text (_cover_sheet);
	if (_last_player_load_directory) {
		root->add_child("LastPlayerLoadDirectory")->add_child_text(_last_player_load_directory->string());
	}
	/* [XML] LastKDMWriteType Last type of KDM-write: <code>flat</code> for a flat file, <code>folder</code> for a folder or <code>zip</code> for a ZIP file. */
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
	/* [XML] LastDKDMWriteType Last type of DKDM-write: <code>file</code> for a file, <code>internal</code> to add to DCP-o-matic's list. */
	if (_last_dkdm_write_type) {
		switch (_last_dkdm_write_type.get()) {
		case DKDM_WRITE_INTERNAL:
			root->add_child("LastDKDMWriteType")->add_child_text("internal");
			break;
		case DKDM_WRITE_FILE:
			root->add_child("LastDKDMWriteType")->add_child_text("file");
			break;
		}
	}
	/* [XML] FramesInMemoryMultiplier value to multiply the encoding threads count by to get the maximum number of
	   frames to be held in memory at once.
	*/
	root->add_child("FramesInMemoryMultiplier")->add_child_text(raw_convert<string>(_frames_in_memory_multiplier));

	/* [XML] DecodeReduction power of 2 to reduce DCP images by before decoding in the player. */
	if (_decode_reduction) {
		root->add_child("DecodeReduction")->add_child_text(raw_convert<string>(_decode_reduction.get()));
	}

	/* [XML] DefaultNotify 1 to default jobs to notify when complete, otherwise 0. */
	root->add_child("DefaultNotify")->add_child_text(_default_notify ? "1" : "0");

	/* [XML] Notification 1 if a notification type is enabled, otherwise 0. */
	for (int i = 0; i < NOTIFICATION_COUNT; ++i) {
		xmlpp::Element* e = root->add_child ("Notification");
		e->set_attribute("id", raw_convert<string>(i));
		e->add_child_text (_notification[i] ? "1" : "0");
	}

	if (_barco_username) {
		/* [XML] BarcoUsername Username for logging into Barco's servers when downloading server certificates. */
		root->add_child("BarcoUsername")->add_child_text(*_barco_username);
	}
	if (_barco_password) {
		/* [XML] BarcoPassword Password for logging into Barco's servers when downloading server certificates. */
		root->add_child("BarcoPassword")->add_child_text(*_barco_password);
	}

	if (_christie_username) {
		/* [XML] ChristieUsername Username for logging into Christie's servers when downloading server certificates. */
		root->add_child("ChristieUsername")->add_child_text(*_christie_username);
	}
	if (_christie_password) {
		/* [XML] ChristiePassword Password for logging into Christie's servers when downloading server certificates. */
		root->add_child("ChristiePassword")->add_child_text(*_christie_password);
	}

	if (_gdc_username) {
		/* [XML] GDCUsername Username for logging into GDC's servers when downloading server certificates. */
		root->add_child("GDCUsername")->add_child_text(*_gdc_username);
	}
	if (_gdc_password) {
		/* [XML] GDCPassword Password for logging into GDC's servers when downloading server certificates. */
		root->add_child("GDCPassword")->add_child_text(*_gdc_password);
	}

	/* [XML] PlayerMode <code>window</code> for a single window, <code>full</code> for full-screen and <code>dual</code> for full screen playback
	   with separate (advanced) controls.
	*/
	switch (_player_mode) {
	case PLAYER_MODE_WINDOW:
		root->add_child("PlayerMode")->add_child_text("window");
		break;
	case PLAYER_MODE_FULL:
		root->add_child("PlayerMode")->add_child_text("full");
		break;
	case PLAYER_MODE_DUAL:
		root->add_child("PlayerMode")->add_child_text("dual");
		break;
	}

	/* [XML] ImageDisplay Screen number to put image on in dual-screen player mode. */
	root->add_child("ImageDisplay")->add_child_text(raw_convert<string>(_image_display));
	switch (_video_view_type) {
	case VIDEO_VIEW_SIMPLE:
		root->add_child("VideoViewType")->add_child_text("simple");
		break;
	case VIDEO_VIEW_OPENGL:
		root->add_child("VideoViewType")->add_child_text("opengl");
		break;
	}
	/* [XML] RespectKDMValidityPeriods 1 to refuse to use KDMs that are out of date, 0 to ignore KDM dates. */
	root->add_child("RespectKDMValidityPeriods")->add_child_text(_respect_kdm_validity_periods ? "1" : "0");
	if (_player_debug_log_file) {
		/* [XML] PlayerLogFile Filename to use for player debug logs. */
		root->add_child("PlayerDebugLogFile")->add_child_text(_player_debug_log_file->string());
	}
	if (_player_content_directory) {
		/* [XML] PlayerContentDirectory Directory to use for player content in the dual-screen mode. */
		root->add_child("PlayerContentDirectory")->add_child_text(_player_content_directory->string());
	}
	if (_player_playlist_directory) {
		/* [XML] PlayerPlaylistDirectory Directory to use for player playlists in the dual-screen mode. */
		root->add_child("PlayerPlaylistDirectory")->add_child_text(_player_playlist_directory->string());
	}
	if (_player_kdm_directory) {
		/* [XML] PlayerKDMDirectory Directory to use for player KDMs in the dual-screen mode. */
		root->add_child("PlayerKDMDirectory")->add_child_text(_player_kdm_directory->string());
	}
	if (_audio_mapping) {
		_audio_mapping->as_xml (root->add_child("AudioMapping"));
	}
	for (auto const& i: _custom_languages) {
		root->add_child("CustomLanguage")->add_child_text(i.to_string());
	}
	for (auto const& initial: _initial_paths) {
		if (initial.second) {
			root->add_child(initial.first)->add_child_text(initial.second->string());
		}
	}
	root->add_child("UseISDCFNameByDefault")->add_child_text(_use_isdcf_name_by_default ? "1" : "0");
	root->add_child("WriteKDMsToDisk")->add_child_text(_write_kdms_to_disk ? "1" : "0");
	root->add_child("EmailKDMs")->add_child_text(_email_kdms ? "1" : "0");
	root->add_child("DefaultKDMType")->add_child_text(dcp::formulation_to_string(_default_kdm_type));
	root->add_child("AutoCropThreshold")->add_child_text(raw_convert<string>(_auto_crop_threshold));
	if (_last_release_notes_version) {
		root->add_child("LastReleaseNotesVersion")->add_child_text(*_last_release_notes_version);
	}
	if (_main_divider_sash_position) {
		root->add_child("MainDividerSashPosition")->add_child_text(raw_convert<string>(*_main_divider_sash_position));
	}
	if (_main_content_divider_sash_position) {
		root->add_child("MainContentDividerSashPosition")->add_child_text(raw_convert<string>(*_main_content_divider_sash_position));
	}

	root->add_child("DefaultAddFileLocation")->add_child_text(
		_default_add_file_location == DefaultAddFileLocation::SAME_AS_LAST_TIME ? "last" : "project"
		);

	/* [XML] AllowSMPTEBv20 1 to allow the user to choose SMPTE (Bv2.0 only) as a standard, otherwise 0 */
	root->add_child("AllowSMPTEBv20")->add_child_text(_allow_smpte_bv20 ? "1" : "0");
	/* [XML] ISDCFNamePartLength Maximum length of the "name" part of an ISDCF name, which should be 14 according to the standard */
	root->add_child("ISDCFNamePartLength")->add_child_text(raw_convert<string>(_isdcf_name_part_length));

#ifdef DCPOMATIC_GROK
	if (_grok) {
		_grok->as_xml(root->add_child("Grok"));
	}
#endif

	_export.write(root->add_child("Export"));

	auto target = config_write_file();

	try {
		auto const s = doc.write_to_string_formatted ();
		boost::filesystem::path tmp (string(target.string()).append(".tmp"));
		dcp::File f(tmp, "w");
		if (!f) {
			throw FileError (_("Could not open file for writing"), tmp);
		}
		f.checked_write(s.c_str(), s.bytes());
		f.close();
		dcp::filesystem::remove(target);
		dcp::filesystem::rename(tmp, target);
	} catch (xmlpp::exception& e) {
		string s = e.what ();
		trim (s);
		throw FileError (s, target);
	}
}


template <class T>
void
write_file (string root_node, string node, string version, list<shared_ptr<T>> things, boost::filesystem::path file)
{
	xmlpp::Document doc;
	auto root = doc.create_root_node (root_node);
	root->add_child("Version")->add_child_text(version);

	for (auto i: things) {
		i->as_xml (root->add_child(node));
	}

	try {
		doc.write_to_file_formatted (file.string() + ".tmp");
		dcp::filesystem::remove(file);
		dcp::filesystem::rename(file.string() + ".tmp", file);
	} catch (xmlpp::exception& e) {
		string s = e.what ();
		trim (s);
		throw FileError (s, file);
	}
}


void
Config::write_cinemas () const
{
	write_file ("Cinemas", "Cinema", "1", _cinemas, _cinemas_file);
}


void
Config::write_dkdm_recipients () const
{
	write_file ("DKDMRecipients", "DKDMRecipient", "1", _dkdm_recipients, _dkdm_recipients_file);
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
	auto const e = dcp::filesystem::exists(*dir, ec);
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
Config::set_notification_email_to_default ()
{
	_notification_subject = _("DCP-o-matic notification");

	_notification_email = _(
		"$JOB_NAME: $JOB_STATUS"
		);
}

void
Config::reset_kdm_email ()
{
	set_kdm_email_to_default ();
	changed ();
}

void
Config::reset_notification_email ()
{
	set_notification_email_to_default ();
	changed ();
}

void
Config::set_cover_sheet_to_default ()
{
	_cover_sheet = _(
		"$CPL_NAME\n\n"
		"CPL Filename: $CPL_FILENAME\n"
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
	add_to_history_internal (_history, p);
}

/** Remove non-existent items from the history */
void
Config::clean_history ()
{
	clean_history_internal (_history);
}

void
Config::add_to_player_history (boost::filesystem::path p)
{
	add_to_history_internal (_player_history, p);
}

/** Remove non-existent items from the player history */
void
Config::clean_player_history ()
{
	clean_history_internal (_player_history);
}

void
Config::add_to_history_internal (vector<boost::filesystem::path>& h, boost::filesystem::path p)
{
	/* Remove existing instances of this path in the history */
	h.erase (remove (h.begin(), h.end(), p), h.end ());

	h.insert (h.begin (), p);
	if (h.size() > HISTORY_SIZE) {
		h.pop_back ();
	}

	changed (HISTORY);
}

void
Config::clean_history_internal (vector<boost::filesystem::path>& h)
{
	auto old = h;
	h.clear ();
	for (auto i: old) {
		try {
			if (dcp::filesystem::is_directory(i)) {
				h.push_back (i);
			}
		} catch (...) {
			/* We couldn't find out if it's a directory for some reason; just ignore it */
		}
	}
}


bool
Config::have_existing (string file)
{
	return dcp::filesystem::exists(read_path(file));
}


void
Config::read_cinemas (cxml::Document const & f)
{
	_cinemas.clear ();
	for (auto i: f.node_children("Cinema")) {
		/* Slightly grotty two-part construction of Cinema here so that we can use
		   shared_from_this.
		*/
		auto cinema = make_shared<Cinema>(i);
		cinema->read_screens (i);
		_cinemas.push_back (cinema);
	}
}

void
Config::set_cinemas_file (boost::filesystem::path file)
{
	if (file == _cinemas_file) {
		return;
	}

	_cinemas_file = file;

	if (dcp::filesystem::exists(_cinemas_file)) {
		/* Existing file; read it in */
		cxml::Document f ("Cinemas");
		f.read_file(dcp::filesystem::fix_long_path(_cinemas_file));
		read_cinemas (f);
	}

	changed (CINEMAS);
	changed (OTHER);
}


void
Config::read_dkdm_recipients (cxml::Document const & f)
{
	_dkdm_recipients.clear ();
	for (auto i: f.node_children("DKDMRecipient")) {
		_dkdm_recipients.push_back (make_shared<DKDMRecipient>(i));
	}
}


void
Config::save_template (shared_ptr<const Film> film, string name) const
{
	film->write_template (template_write_path(name));
}


list<string>
Config::templates () const
{
	if (!dcp::filesystem::exists(read_path("templates"))) {
		return {};
	}

	list<string> n;
	for (auto const& i: dcp::filesystem::directory_iterator(read_path("templates"))) {
		n.push_back (i.path().filename().string());
	}
	return n;
}

bool
Config::existing_template (string name) const
{
	return dcp::filesystem::exists(template_read_path(name));
}


boost::filesystem::path
Config::template_read_path (string name) const
{
	return read_path("templates") / tidy_for_filename (name);
}


boost::filesystem::path
Config::template_write_path (string name) const
{
	return write_path("templates") / tidy_for_filename (name);
}


void
Config::rename_template (string old_name, string new_name) const
{
	dcp::filesystem::rename(template_read_path(old_name), template_write_path(new_name));
}

void
Config::delete_template (string name) const
{
	dcp::filesystem::remove(template_write_path(name));
}

/** @return Path to the config.xml containing the actual settings, following a link if required */
boost::filesystem::path
config_file (boost::filesystem::path main)
{
	cxml::Document f ("Config");
	if (!dcp::filesystem::exists(main)) {
		/* It doesn't exist, so there can't be any links; just return it */
		return main;
	}

	/* See if there's a link */
	try {
		f.read_file(dcp::filesystem::fix_long_path(main));
		auto link = f.optional_string_child("Link");
		if (link) {
			return *link;
		}
	} catch (xmlpp::exception& e) {
		/* There as a problem reading the main configuration file,
		   so there can't be a link.
		*/
	}

	return main;
}


boost::filesystem::path
Config::config_read_file ()
{
	return config_file (read_path("config.xml"));
}


boost::filesystem::path
Config::config_write_file ()
{
	return config_file (write_path("config.xml"));
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
		doc.write_to_file_formatted(write_path("config.xml").string());
	} catch (xmlpp::exception& e) {
		string s = e.what ();
		trim (s);
		throw FileError (s, write_path("config.xml"));
	}
}

void
Config::copy_and_link (boost::filesystem::path new_file) const
{
	write ();
	dcp::filesystem::copy_file(config_read_file(), new_file, boost::filesystem::copy_option::overwrite_if_exists);
	link (new_file);
}

bool
Config::have_write_permission () const
{
	dcp::File f(config_write_file(), "r+");
	return static_cast<bool>(f);
}

/** @param  output_channels Number of output channels in use.
 *  @return Audio mapping for this output channel count (may be a default).
 */
AudioMapping
Config::audio_mapping (int output_channels)
{
	if (!_audio_mapping || _audio_mapping->output_channels() != output_channels) {
		/* Set up a default */
		_audio_mapping = AudioMapping (MAX_DCP_AUDIO_CHANNELS, output_channels);
		if (output_channels == 2) {
			/* Special case for stereo output.
			   Map so that Lt = L(-3dB) + Ls(-3dB) + C(-6dB) + Lfe(-10dB)
			   Rt = R(-3dB) + Rs(-3dB) + C(-6dB) + Lfe(-10dB)
			*/
			_audio_mapping->set (dcp::Channel::LEFT,   0, 1 / sqrt(2));  // L   -> Lt
			_audio_mapping->set (dcp::Channel::RIGHT,  1, 1 / sqrt(2));  // R   -> Rt
			_audio_mapping->set (dcp::Channel::CENTRE, 0, 1 / 2.0);      // C   -> Lt
			_audio_mapping->set (dcp::Channel::CENTRE, 1, 1 / 2.0);      // C   -> Rt
			_audio_mapping->set (dcp::Channel::LFE,    0, 1 / sqrt(10)); // Lfe -> Lt
			_audio_mapping->set (dcp::Channel::LFE,    1, 1 / sqrt(10)); // Lfe -> Rt
			_audio_mapping->set (dcp::Channel::LS,     0, 1 / sqrt(2));  // Ls  -> Lt
			_audio_mapping->set (dcp::Channel::RS,     1, 1 / sqrt(2));  // Rs  -> Rt
		} else {
			/* 1:1 mapping */
			for (int i = 0; i < min (MAX_DCP_AUDIO_CHANNELS, output_channels); ++i) {
				_audio_mapping->set (i, i, 1);
			}
		}
	}

	return *_audio_mapping;
}

void
Config::set_audio_mapping (AudioMapping m)
{
	_audio_mapping = m;
	changed (AUDIO_MAPPING);
}

void
Config::set_audio_mapping_to_default ()
{
	DCPOMATIC_ASSERT (_audio_mapping);
	auto const ch = _audio_mapping->output_channels ();
	_audio_mapping = boost::none;
	_audio_mapping = audio_mapping (ch);
	changed (AUDIO_MAPPING);
}


void
Config::add_custom_language (dcp::LanguageTag tag)
{
	if (find(_custom_languages.begin(), _custom_languages.end(), tag) == _custom_languages.end()) {
		_custom_languages.push_back (tag);
		changed ();
	}
}


optional<Config::BadReason>
Config::check_certificates () const
{
	optional<BadReason> bad;

	for (auto const& i: _signer_chain->unordered()) {
		if (i.has_utf8_strings()) {
			bad = BAD_SIGNER_UTF8_STRINGS;
		}
		if ((i.not_after().year() - i.not_before().year()) > 15) {
			bad = BAD_SIGNER_VALIDITY_TOO_LONG;
		}
		if (dcp::escape_digest(i.subject_dn_qualifier()) != dcp::public_key_digest(i.public_key())) {
			bad = BAD_SIGNER_DN_QUALIFIER;
		}
	}

	if (!_signer_chain->chain_valid() || !_signer_chain->private_key_valid()) {
		bad = BAD_SIGNER_INCONSISTENT;
	}

	if (!_decryption_chain->chain_valid() || !_decryption_chain->private_key_valid()) {
		bad = BAD_DECRYPTION_INCONSISTENT;
	}

	return bad;
}


void
save_all_config_as_zip (boost::filesystem::path zip_file)
{
	Zipper zipper (zip_file);

	auto config = Config::instance();
	zipper.add ("config.xml", dcp::file_to_string(config->config_read_file()));
	if (dcp::filesystem::exists(config->cinemas_file())) {
		zipper.add ("cinemas.xml", dcp::file_to_string(config->cinemas_file()));
	}
	if (dcp::filesystem::exists(config->dkdm_recipients_file())) {
		zipper.add ("dkdm_recipients.xml", dcp::file_to_string(config->dkdm_recipients_file()));
	}

	zipper.close ();
}


void
Config::set_initial_path(string id, boost::filesystem::path path)
{
	auto iter = _initial_paths.find(id);
	DCPOMATIC_ASSERT(iter != _initial_paths.end());
	iter->second = path;
	changed();
}


optional<boost::filesystem::path>
Config::initial_path(string id) const
{
	auto iter = _initial_paths.find(id);
	DCPOMATIC_ASSERT(iter != _initial_paths.end());
	return iter->second;
}


#ifdef DCPOMATIC_GROK

Config::Grok::Grok(cxml::ConstNodePtr node)
	: enable(node->bool_child("Enable"))
	, binary_location(node->string_child("BinaryLocation"))
	, selected(node->number_child<int>("Selected"))
	, licence_server(node->string_child("LicenceServer"))
	, licence_port(node->number_child<int>("LicencePort"))
	, licence(node->string_child("Licence"))
{

}


void
Config::Grok::as_xml(xmlpp::Element* node) const
{
	node->add_child("BinaryLocation")->add_child_text(binary_location.string());
	node->add_child("Enable")->add_child_text((enable ? "1" : "0"));
	node->add_child("Selected")->add_child_text(raw_convert<string>(selected));
	node->add_child("LicenceServer")->add_child_text(licence_server);
	node->add_child("LicencePort")->add_child_text(raw_convert<string>(licence_port));
	node->add_child("Licence")->add_child_text(licence);
}


void
Config::set_grok(Grok const& grok)
{
	_grok = grok;
	changed(GROK);
}

#endif
