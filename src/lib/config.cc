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


#include "cinema_list.h"
#include "colour_conversion.h"
#include "config.h"
#include "constants.h"
#include "cross.h"
#include "dcp_content_type.h"
#include "dkdm_recipient_list.h"
#include "dkdm_wrapper.h"
#include "film.h"
#include "filter.h"
#include "log.h"
#include "ratio.h"
#include "unzipper.h"
#include "variant.h"
#include "zipper.h"
#include <dcp/certificate_chain.h>
#include <dcp/name_format.h>
#include <dcp/raw_convert.h>
#include <libcxml/cxml.h>
#include <glib.h>
#include <libxml++/libxml++.h>
#include <fmt/format.h>
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

#ifdef DCPOMATIC_GROK
auto constexpr default_grok_licence_server = "https://grokcompression.com/api/register";
#endif


/** Construct default configuration */
Config::Config()
        /* DKDMs are not considered a thing to reset on set_defaults() */
	: _dkdms(new DKDMGroup("root"))
	, _default_kdm_duration(1, RoughDuration::Unit::WEEKS)
	, _export(this)
{
	set_defaults();
}

void
Config::set_defaults()
{
	_master_encoding_threads = max(2U, boost::thread::hardware_concurrency());
	_server_encoding_threads = max(2U, boost::thread::hardware_concurrency());
	_server_port_base = 6192;
	_use_any_servers = true;
	_servers.clear();
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
	_language = optional<string>();
	_default_still_length = 10;
	_default_dcp_content_type = DCPContentType::from_isdcf_name("FTR");
	_default_dcp_audio_channels = 8;
	_default_video_bit_rate[VideoEncoding::JPEG2000] = 150000000;
	_default_video_bit_rate[VideoEncoding::MPEG2] = 5000000;
	_default_audio_delay = 0;
	_default_interop = false;
	_default_metadata.clear();
	_upload_after_make_dcp = false;
	_mail_server = "";
	_mail_port = 25;
	_mail_protocol = EmailProtocol::AUTO;
	_mail_user = "";
	_mail_password = "";
	_kdm_from = "";
	_kdm_cc.clear();
	_kdm_bcc = "";
	_notification_from = "";
	_notification_to = "";
	_notification_cc.clear();
	_notification_bcc = "";
	_check_for_updates = false;
	_check_for_test_updates = false;
	_maximum_video_bit_rate[VideoEncoding::JPEG2000] = 250000000;
	_maximum_video_bit_rate[VideoEncoding::MPEG2] = 50000000;
	_log_types = LogEntry::TYPE_GENERAL | LogEntry::TYPE_WARNING | LogEntry::TYPE_ERROR | LogEntry::TYPE_DISK;
	_analyse_ebur128 = true;
	_automatic_audio_analysis = false;
#ifdef DCPOMATIC_WINDOWS
	_win32_console = false;
#endif
	/* At the moment we don't write these files anywhere new after a version change, so they will be read from
	 * ~/.config/dcpomatic2 (or equivalent) and written back there.
	 */
	_cinemas_file = read_path("cinemas.sqlite3");
	_dkdm_recipients_file = read_path("dkdm_recipients.sqlite3");
	_show_hints_before_make_dcp = true;
	_confirm_kdm_email = true;
	_kdm_container_name_format = dcp::NameFormat("KDM_%f_%c");
	_kdm_filename_format = dcp::NameFormat("KDM_%f_%c_%s");
	_dkdm_filename_format = dcp::NameFormat("DKDM_%f_%r");
	_dcp_metadata_filename_format = dcp::NameFormat("%t");
	_dcp_asset_filename_format = dcp::NameFormat("%t");
	_jump_to_selected = true;
	for (int i = 0; i < NAG_COUNT; ++i) {
		_nagged[i] = false;
	}
	_sound = true;
	_sound_output = optional<string>();
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
	_player_mode = PlayerMode::WINDOW;
	_player_restricted_menus = false;
	_playlist_editor_restricted_menus = false;
	_image_display = 0;
	_video_view_type = VIDEO_VIEW_SIMPLE;
	_respect_kdm_validity_periods = true;
	_player_debug_log_file = boost::none;
	_kdm_debug_log_file = boost::none;
	_player_content_directory = boost::none;
	_player_playlist_directory = boost::none;
	_player_kdm_directory = boost::none;
	_audio_mapping = boost::none;
	_custom_languages.clear();
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
	_initial_paths["Preferences"] = boost::none;
	_initial_paths["SaveVerificationReport"] = boost::none;
	_initial_paths["CopySettingsPath"] = boost::none;
	_use_isdcf_name_by_default = true;
	_write_kdms_to_disk = true;
	_email_kdms = false;
	_default_kdm_type = dcp::Formulation::MODIFIED_TRANSITIONAL_1;
	_default_kdm_duration = RoughDuration(1, RoughDuration::Unit::WEEKS);
	_auto_crop_threshold = 0.1;
	_last_release_notes_version = boost::none;
	_allow_smpte_bv20 = false;
	_isdcf_name_part_length = 14;
	_enable_player_http_server = false;
	_player_http_server_port = 8080;
	_relative_paths = false;
	_layout_for_short_screen = false;

	_allowed_dcp_frame_rates.clear();
	_allowed_dcp_frame_rates.push_back(24);
	_allowed_dcp_frame_rates.push_back(25);
	_allowed_dcp_frame_rates.push_back(30);
	_allowed_dcp_frame_rates.push_back(48);
	_allowed_dcp_frame_rates.push_back(50);
	_allowed_dcp_frame_rates.push_back(60);

	set_kdm_email_to_default();
	set_notification_email_to_default();
	set_cover_sheet_to_default();

#ifdef DCPOMATIC_GROK
	_grok = {};
#endif

	_main_divider_sash_position = {};
	_main_content_divider_sash_position = {};

	_export.set_defaults();
}

void
Config::restore_defaults()
{
	Config::instance()->set_defaults();
	Config::instance()->changed();
}

shared_ptr<dcp::CertificateChain>
Config::create_certificate_chain()
{
	return make_shared<dcp::CertificateChain>(
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
Config::backup()
{
	using namespace boost::filesystem;

	auto copy_adding_number = [](path const& path_to_copy) {

		auto add_number = [](path const& path, int number) {
			return fmt::format("{}.{}", path.string(), number);
		};

		int n = 1;
		while (n < 100 && exists(add_number(path_to_copy, n))) {
			++n;
		}
		boost::system::error_code ec;
		copy_file(path_to_copy, add_number(path_to_copy, n), ec);
	};

	/* Make a backup copy of any config.xml, cinemas.sqlite3, dkdm_recipients.sqlite3 that we might be about
	 * to write over.  This is more intended for the situation where we have a corrupted config.xml,
	 * and decide to overwrite it with a new one (possibly losing important details in the corrupted
	 * file).  But we might as well back up the other files while we're about it.
	 */

	/* This uses the State::write_path stuff so, e.g. for a current version 2.16 we might copy
	 * ~/.config/dcpomatic2/2.16/config.xml to ~/.config/dcpomatic2/2.16/config.xml.1
	 */
	copy_adding_number(config_write_file());

	/* These do not use State::write_path, so whatever path is in the Config we will copy
	 * adding a number.
	 */
	copy_adding_number(_cinemas_file);
	copy_adding_number(_dkdm_recipients_file);
}

void
Config::read()
try
{
	cxml::Document f("Config");
	f.read_file(dcp::filesystem::fix_long_path(config_read_file()));

	auto version = f.optional_number_child<int>("Version");
	if (version && *version < _current_version) {
		/* Back up the old config before we re-write it in a back-incompatible way */
		backup();
	}

	if (f.optional_number_child<int>("NumLocalEncodingThreads")) {
		_master_encoding_threads = _server_encoding_threads = f.optional_number_child<int>("NumLocalEncodingThreads").get();
	} else {
		_master_encoding_threads = f.number_child<int>("MasterEncodingThreads");
		_server_encoding_threads = f.number_child<int>("ServerEncodingThreads");
	}

	_default_directory = f.optional_string_child("DefaultDirectory");
	if (_default_directory && _default_directory->empty()) {
		/* We used to store an empty value for this to mean "none set" */
		_default_directory = boost::optional<boost::filesystem::path>();
	}

	auto b = f.optional_number_child<int>("ServerPort");
	if (!b) {
		b = f.optional_number_child<int>("ServerPortBase");
	}
	_server_port_base = b.get();

	auto u = f.optional_bool_child("UseAnyServers");
	_use_any_servers = u.get_value_or(true);

	for (auto i: f.node_children("Server")) {
		if (i->node_children("HostName").size() == 1) {
			_servers.push_back(i->string_child("HostName"));
		} else {
			_servers.push_back(i->content());
		}
	}

	_only_servers_encode = f.optional_bool_child("OnlyServersEncode").get_value_or(false);
	_tms_protocol = static_cast<FileTransferProtocol>(f.optional_number_child<int>("TMSProtocol").get_value_or(static_cast<int>(FileTransferProtocol::SCP)));
	_tms_passive = f.optional_bool_child("TMSPassive").get_value_or(true);
	_tms_ip = f.string_child("TMSIP");
	_tms_path = f.string_child("TMSPath");
	_tms_user = f.string_child("TMSUser");
	_tms_password = f.string_child("TMSPassword");

	_language = f.optional_string_child("Language");

	_default_dcp_content_type = DCPContentType::from_isdcf_name(f.optional_string_child("DefaultDCPContentType").get_value_or("FTR"));
	_default_dcp_audio_channels = f.optional_number_child<int>("DefaultDCPAudioChannels").get_value_or(6);

	if (f.optional_string_child("DCPMetadataIssuer")) {
		_dcp_issuer = f.string_child("DCPMetadataIssuer");
	} else if (f.optional_string_child("DCPIssuer")) {
		_dcp_issuer = f.string_child("DCPIssuer");
	}

	auto up = f.optional_bool_child("UploadAfterMakeDCP");
	if (!up) {
		up = f.optional_bool_child("DefaultUploadAfterMakeDCP");
	}
	_upload_after_make_dcp = up.get_value_or(false);
	_dcp_creator = f.optional_string_child("DCPCreator").get_value_or("");
	_dcp_company_name = f.optional_string_child("DCPCompanyName").get_value_or("");
	_dcp_product_name = f.optional_string_child("DCPProductName").get_value_or("");
	_dcp_product_version = f.optional_string_child("DCPProductVersion").get_value_or("");
	_dcp_j2k_comment = f.optional_string_child("DCPJ2KComment").get_value_or("");

	_default_still_length = f.optional_number_child<int>("DefaultStillLength").get_value_or(10);
	if (auto j2k = f.optional_number_child<int>("DefaultJ2KBandwidth")) {
		_default_video_bit_rate[VideoEncoding::JPEG2000] = *j2k;
	} else {
		_default_video_bit_rate[VideoEncoding::JPEG2000] = f.optional_number_child<int64_t>("DefaultJ2KVideoBitRate").get_value_or(200000000);
	}
	_default_video_bit_rate[VideoEncoding::MPEG2] = f.optional_number_child<int64_t>("DefaultMPEG2VideoBitRate").get_value_or(5000000);
	_default_audio_delay = f.optional_number_child<int>("DefaultAudioDelay").get_value_or(0);
	_default_interop = f.optional_bool_child("DefaultInterop").get_value_or(false);

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

	_mail_server = f.string_child("MailServer");
	_mail_port = f.optional_number_child<int>("MailPort").get_value_or(25);

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

	_mail_user = f.optional_string_child("MailUser").get_value_or("");
	_mail_password = f.optional_string_child("MailPassword").get_value_or("");

	_kdm_subject = f.optional_string_child("KDMSubject").get_value_or(_("KDM delivery: $CPL_NAME"));
	_kdm_from = f.string_child("KDMFrom");
	for (auto i: f.node_children("KDMCC")) {
		if (!i->content().empty()) {
			_kdm_cc.push_back(i->content());
		}
	}
	_kdm_bcc = f.optional_string_child("KDMBCC").get_value_or("");
	_kdm_email = f.string_child("KDMEmail");

	_notification_subject = f.optional_string_child("NotificationSubject").get_value_or(variant::insert_dcpomatic(_("{} notification")));
	_notification_from = f.optional_string_child("NotificationFrom").get_value_or("");
	_notification_to = f.optional_string_child("NotificationTo").get_value_or("");
	for (auto i: f.node_children("NotificationCC")) {
		if (!i->content().empty()) {
			_notification_cc.push_back(i->content());
		}
	}
	_notification_bcc = f.optional_string_child("NotificationBCC").get_value_or("");
	if (f.optional_string_child("NotificationEmail")) {
		_notification_email = f.string_child("NotificationEmail");
	}

	_check_for_updates = f.optional_bool_child("CheckForUpdates").get_value_or(false);
	_check_for_test_updates = f.optional_bool_child("CheckForTestUpdates").get_value_or(false);

	if (auto j2k = f.optional_number_child<int>("MaximumJ2KBandwidth")) {
		_maximum_video_bit_rate[VideoEncoding::JPEG2000] = *j2k;
	} else {
		_maximum_video_bit_rate[VideoEncoding::JPEG2000] = f.optional_number_child<int64_t>("MaximumJ2KVideoBitRate").get_value_or(250000000);
	}
	_maximum_video_bit_rate[VideoEncoding::MPEG2] = f.optional_number_child<int64_t>("MaximumMPEG2VideoBitRate").get_value_or(50000000);
	_allow_any_dcp_frame_rate = f.optional_bool_child("AllowAnyDCPFrameRate").get_value_or(false);
	_allow_any_container = f.optional_bool_child("AllowAnyContainer").get_value_or(false);
	_allow_96khz_audio = f.optional_bool_child("Allow96kHzAudio").get_value_or(false);
	_use_all_audio_channels = f.optional_bool_child("UseAllAudioChannels").get_value_or(false);
	_show_experimental_audio_processors = f.optional_bool_child("ShowExperimentalAudioProcessors").get_value_or(false);

	_log_types = f.optional_number_child<int>("LogTypes").get_value_or(LogEntry::TYPE_GENERAL | LogEntry::TYPE_WARNING | LogEntry::TYPE_ERROR);
	_analyse_ebur128 = f.optional_bool_child("AnalyseEBUR128").get_value_or(true);
	_automatic_audio_analysis = f.optional_bool_child("AutomaticAudioAnalysis").get_value_or(false);
#ifdef DCPOMATIC_WINDOWS
	_win32_console = f.optional_bool_child("Win32Console").get_value_or(false);
#endif

	for (auto i: f.node_children("History")) {
		_history.push_back(i->content());
	}

	for (auto i: f.node_children("PlayerHistory")) {
		_player_history.push_back(i->content());
	}

	auto signer = f.optional_node_child("Signer");
	if (signer) {
		auto c = make_shared<dcp::CertificateChain>();
		/* Read the signing certificates and private key in from the config file */
		for (auto i: signer->node_children("Certificate")) {
			c->add(dcp::Certificate(i->content()));
		}
		c->set_key(signer->string_child("PrivateKey"));
		_signer_chain = c;
	} else {
		/* Make a new set of signing certificates and key */
		_signer_chain = create_certificate_chain();
	}

	auto decryption = f.optional_node_child("Decryption");
	if (decryption) {
		auto c = make_shared<dcp::CertificateChain>();
		for (auto i: decryption->node_children("Certificate")) {
			c->add(dcp::Certificate(i->content ()));
		}
		c->set_key(decryption->string_child("PrivateKey"));
		_decryption_chain = c;
	} else {
		_decryption_chain = create_certificate_chain();
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

	auto bad = check_certificates();
	if (bad) {
		auto const remake = Bad(*bad);
		if (remake && *remake) {
			switch (*bad) {
			case BAD_SIGNER_UTF8_STRINGS:
			case BAD_SIGNER_INCONSISTENT:
			case BAD_SIGNER_VALIDITY_TOO_LONG:
			case BAD_SIGNER_DN_QUALIFIER:
				_signer_chain = create_certificate_chain();
				break;
			case BAD_DECRYPTION_INCONSISTENT:
				_decryption_chain = create_certificate_chain();
				break;
			}
		}
	}

	if (f.optional_node_child("DKDMGroup")) {
		/* New-style: all DKDMs in a group */
		_dkdms = dynamic_pointer_cast<DKDMGroup>(DKDMBase::read(f.node_child("DKDMGroup")));
	} else {
		/* Old-style: one or more DKDM nodes */
		_dkdms = make_shared<DKDMGroup>("root");
		for (auto i: f.node_children("DKDM")) {
			_dkdms->add(DKDMBase::read(i));
		}
	}
	_cinemas_file = f.optional_string_child("CinemasFile").get_value_or(read_path("cinemas.sqlite3").string());
	_dkdm_recipients_file = f.optional_string_child("DKDMRecipientsFile").get_value_or(read_path("dkdm_recipients.sqlite3").string());
	_show_hints_before_make_dcp = f.optional_bool_child("ShowHintsBeforeMakeDCP").get_value_or(true);
	_confirm_kdm_email = f.optional_bool_child("ConfirmKDMEmail").get_value_or(true);
	_kdm_container_name_format = dcp::NameFormat(f.optional_string_child("KDMContainerNameFormat").get_value_or("KDM %f %c"));
	_kdm_filename_format = dcp::NameFormat(f.optional_string_child("KDMFilenameFormat").get_value_or("KDM_%f_%c_%s"));
	_dkdm_filename_format = dcp::NameFormat(f.optional_string_child("DKDMFilenameFormat").get_value_or("DKDM_%f_%r"));
	if (_dkdm_filename_format.specification() == "DKDM_%f_%c_%s" || _dkdm_filename_format.specification() == "DKDM %f %c %s") {
		/* The DKDM filename format is one of our previous defaults, neither of which make any sense.
		 * Fix to something more useful.
		 */
		_dkdm_filename_format = dcp::NameFormat("DKDM_%f_%r");
	}
	_dcp_metadata_filename_format = dcp::NameFormat(f.optional_string_child("DCPMetadataFilenameFormat").get_value_or("%t"));
	_dcp_asset_filename_format = dcp::NameFormat(f.optional_string_child("DCPAssetFilenameFormat").get_value_or("%t"));
	_jump_to_selected = f.optional_bool_child("JumpToSelected").get_value_or(true);
	/* The variable was renamed but not the XML tag */
	_sound = f.optional_bool_child("PreviewSound").get_value_or(true);
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
		_player_mode = PlayerMode::WINDOW;
	} else if (pm && *pm == "full") {
		_player_mode = PlayerMode::FULL;
	} else if (pm && *pm == "dual") {
		_player_mode = PlayerMode::DUAL;
	}

	_player_restricted_menus = f.optional_bool_child("PlayerRestrictedMenus").get_value_or(false);
	_playlist_editor_restricted_menus = f.optional_bool_child("PlaylistEditorRestrictedMenus").get_value_or(false);

	_player_crop_output_ratio = f.optional_number_child<float>("PlayerCropOutputRatio");

	_image_display = f.optional_number_child<int>("ImageDisplay").get_value_or(0);
	auto vc = f.optional_string_child("VideoViewType");
	if (vc && *vc == "opengl") {
		_video_view_type = VIDEO_VIEW_OPENGL;
	} else if (vc && *vc == "simple") {
		_video_view_type = VIDEO_VIEW_SIMPLE;
	}
	_respect_kdm_validity_periods = f.optional_bool_child("RespectKDMValidityPeriods").get_value_or(true);
	_player_debug_log_file = f.optional_string_child("PlayerDebugLogFile");
	_kdm_debug_log_file = f.optional_string_child("KDMDebugLogFile");
	_player_content_directory = f.optional_string_child("PlayerContentDirectory");
	_player_playlist_directory = f.optional_string_child("PlayerPlaylistDirectory");
	_player_kdm_directory = f.optional_string_child("PlayerKDMDirectory");

	if (f.optional_node_child("AudioMapping")) {
		_audio_mapping = AudioMapping(f.node_child("AudioMapping"), Film::current_state_version);
	}

	for (auto i: f.node_children("CustomLanguage")) {
		try {
			/* This will fail if it's called before dcp::init() as it won't recognise the
			 * tag.  That's OK because the Config will be reloaded again later.
			 */
			_custom_languages.push_back(dcp::LanguageTag(i->content()));
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
	_enable_player_http_server = f.optional_bool_child("EnablePlayerHTTPServer").get_value_or(false);
	_player_http_server_port = f.optional_number_child<int>("PlayerHTTPServerPort").get_value_or(8080);
	_relative_paths = f.optional_bool_child("RelativePaths").get_value_or(false);
	_layout_for_short_screen = f.optional_bool_child("LayoutForShortScreen").get_value_or(false);

#ifdef DCPOMATIC_GROK
	if (auto grok = f.optional_node_child("Grok")) {
		_grok = Grok(grok);
	}
#endif

	_export.read(f.optional_node_child("Export"));
}
catch (...) {
	if (have_existing("config.xml")) {
		backup();
		/* We have a config file but it didn't load */
		FailedToLoad(LoadFailure::CONFIG);
	}
	set_defaults();
	/* Make a new set of signing certificates and key */
	_signer_chain = create_certificate_chain();
	/* And similar for decryption of KDMs */
	_decryption_chain = create_certificate_chain();
	write_config();
}


/** @return Singleton instance */
Config *
Config::instance()
{
	if (_instance == nullptr) {
		_instance = new Config;
		_instance->read();

		auto cinemas_file = _instance->cinemas_file();
		if (cinemas_file.extension() == ".xml") {
			auto sqlite = cinemas_file;
			sqlite.replace_extension(".sqlite3");

			_instance->set_cinemas_file(sqlite);

			if (dcp::filesystem::exists(cinemas_file) && !dcp::filesystem::exists(sqlite)) {
				CinemaList cinemas;
				cinemas.read_legacy_file(cinemas_file);
			}
		}

		auto dkdm_recipients_file = _instance->dkdm_recipients_file();
		if (dkdm_recipients_file.extension() == ".xml") {
			auto sqlite = dkdm_recipients_file;
			sqlite.replace_extension(".sqlite3");

			_instance->set_dkdm_recipients_file(sqlite);

			if (dcp::filesystem::exists(dkdm_recipients_file) && !dcp::filesystem::exists(sqlite)) {
				DKDMRecipientList recipients;
				recipients.read_legacy_file(dkdm_recipients_file);
			}
		}
	}

	return _instance;
}

/** Write our configuration to disk */
void
Config::write() const
{
	write_config();
}

void
Config::write_config() const
{
	xmlpp::Document doc;
	auto root = doc.create_root_node("Config");

	/* [XML] Version The version number of the configuration file format. */
	cxml::add_text_child(root, "Version", fmt::to_string(_current_version));
	/* [XML] MasterEncodingThreads Number of encoding threads to use when running as master. */
	cxml::add_text_child(root, "MasterEncodingThreads", fmt::to_string(_master_encoding_threads));
	/* [XML] ServerEncodingThreads Number of encoding threads to use when running as server. */
	cxml::add_text_child(root, "ServerEncodingThreads", fmt::to_string(_server_encoding_threads));
	if (_default_directory) {
		/* [XML:opt] DefaultDirectory Default directory when creating a new film in the GUI. */
		cxml::add_text_child(root, "DefaultDirectory", _default_directory->string());
	}
	/* [XML] ServerPortBase Port number to use for frame encoding requests.  <code>ServerPortBase</code> + 1 and
	   <code>ServerPortBase</code> + 2 are used for querying servers.  <code>ServerPortBase</code> + 3 is used
	   by the batch converter to listen for job requests.
	*/
	cxml::add_text_child(root, "ServerPortBase", fmt::to_string(_server_port_base));
	/* [XML] UseAnyServers 1 to broadcast to look for encoding servers to use, 0 to use only those configured. */
	cxml::add_text_child(root, "UseAnyServers", _use_any_servers ? "1" : "0");

	for (auto i: _servers) {
		/* [XML:opt] Server IP address or hostname of an encoding server to use; you can use as many of these tags
		   as you like.
		*/
		cxml::add_text_child(root, "Server", i);
	}

	/* [XML] OnlyServersEncode 1 to set the master to do decoding of source content no JPEG2000 encoding; all encoding
	   is done by the encoding servers.  0 to set the master to do some encoding as well as coordinating the job.
	*/
	cxml::add_text_child(root, "OnlyServersEncode", _only_servers_encode ? "1" : "0");
	/* [XML] TMSProtocol Protocol to use to copy files to a TMS; 0 to use SCP, 1 for FTP. */
	cxml::add_text_child(root, "TMSProtocol", fmt::to_string(static_cast<int>(_tms_protocol)));
	/* [XML] TMSPassive True to use PASV mode with TMS FTP connections. */
	cxml::add_text_child(root, "TMSPassive", _tms_passive ? "1" : "0");
	/* [XML] TMSIP IP address of TMS. */
	cxml::add_text_child(root, "TMSIP", _tms_ip);
	/* [XML] TMSPath Path on the TMS to copy files to. */
	cxml::add_text_child(root, "TMSPath", _tms_path);
	/* [XML] TMSUser Username to log into the TMS with. */
	cxml::add_text_child(root, "TMSUser", _tms_user);
	/* [XML] TMSPassword Password to log into the TMS with. */
	cxml::add_text_child(root, "TMSPassword", _tms_password);
	if (_language) {
		/* [XML:opt] Language Language to use in the GUI e.g. <code>fr_FR</code>. */
		cxml::add_text_child(root, "Language", _language.get());
	}
	/* [XML] DCPIssuer Issuer text to write into CPL files. */
	cxml::add_text_child(root, "DCPIssuer", _dcp_issuer);
	/* [XML] DCPCreator Creator text to write into CPL files. */
	cxml::add_text_child(root, "DCPCreator", _dcp_creator);
	/* [XML] DCPCompanyName Company name to write into MXF files. */
	cxml::add_text_child(root, "DCPCompanyName", _dcp_company_name);
	/* [XML] DCPProductName Product name to write into MXF files. */
	cxml::add_text_child(root, "DCPProductName", _dcp_product_name);
	/* [XML] DCPProductVersion Product version to write into MXF files. */
	cxml::add_text_child(root, "DCPProductVersion", _dcp_product_version);
	/* [XML] DCPJ2KComment Comment to write into JPEG2000 data. */
	cxml::add_text_child(root, "DCPJ2KComment", _dcp_j2k_comment);
	/* [XML] UploadAfterMakeDCP 1 to upload to a TMS after making a DCP, 0 for no upload. */
	cxml::add_text_child(root, "UploadAfterMakeDCP", _upload_after_make_dcp ? "1" : "0");

	/* [XML] DefaultStillLength Default length (in seconds) for still images in new films. */
	cxml::add_text_child(root, "DefaultStillLength", fmt::to_string(_default_still_length));
	/* [XML] DefaultAudioDelay Default delay to apply to audio (positive moves audio later) in milliseconds. */
	cxml::add_text_child(root, "DefaultAudioDelay", fmt::to_string(_default_audio_delay));
	if (_default_audio_language) {
		/* [XML] DefaultAudioLanguage Default audio language to use for new films */
		cxml::add_text_child(root, "DefaultAudioLanguage", _default_audio_language->as_string());
	}
	if (_default_kdm_directory) {
		/* [XML:opt] DefaultKDMDirectory Default directory to write KDMs to. */
		cxml::add_text_child(root, "DefaultKDMDirectory", _default_kdm_directory->string());
	}
	_default_kdm_duration.as_xml(cxml::add_child(root, "DefaultKDMDuration"));
	/* [XML] MailServer Hostname of SMTP server to use. */
	cxml::add_text_child(root, "MailServer", _mail_server);
	/* [XML] MailPort Port number to use on SMTP server. */
	cxml::add_text_child(root, "MailPort", fmt::to_string(_mail_port));
	/* [XML] MailProtocol Protocol to use on SMTP server (Auto, Plain, STARTTLS or SSL) */
	switch (_mail_protocol) {
	case EmailProtocol::AUTO:
		cxml::add_text_child(root, "MailProtocol", "Auto");
		break;
	case EmailProtocol::PLAIN:
		cxml::add_text_child(root, "MailProtocol", "Plain");
		break;
	case EmailProtocol::STARTTLS:
		cxml::add_text_child(root, "MailProtocol", "STARTTLS");
		break;
	case EmailProtocol::SSL:
		cxml::add_text_child(root, "MailProtocol", "SSL");
		break;
	}
	/* [XML] MailUser Username to use on SMTP server. */
	cxml::add_text_child(root, "MailUser", _mail_user);
	/* [XML] MailPassword Password to use on SMTP server. */
	cxml::add_text_child(root, "MailPassword", _mail_password);

	/* [XML] KDMSubject Subject to use for KDM emails. */
	cxml::add_text_child(root, "KDMSubject", _kdm_subject);
	/* [XML] KDMFrom From address to use for KDM emails. */
	cxml::add_text_child(root, "KDMFrom", _kdm_from);
	for (auto i: _kdm_cc) {
		/* [XML] KDMCC CC address to use for KDM emails; you can use as many of these tags as you like. */
		cxml::add_text_child(root, "KDMCC", i);
	}
	/* [XML] KDMBCC BCC address to use for KDM emails. */
	cxml::add_text_child(root, "KDMBCC", _kdm_bcc);
	/* [XML] KDMEmail Text of KDM email. */
	cxml::add_text_child(root, "KDMEmail", _kdm_email);

	/* [XML] NotificationSubject Subject to use for notification emails. */
	cxml::add_text_child(root, "NotificationSubject", _notification_subject);
	/* [XML] NotificationFrom From address to use for notification emails. */
	cxml::add_text_child(root, "NotificationFrom", _notification_from);
	/* [XML] NotificationFrom To address to use for notification emails. */
	cxml::add_text_child(root, "NotificationTo", _notification_to);
	for (auto i: _notification_cc) {
		/* [XML] NotificationCC CC address to use for notification emails; you can use as many of these tags as you like. */
		cxml::add_text_child(root, "NotificationCC", i);
	}
	/* [XML] NotificationBCC BCC address to use for notification emails. */
	cxml::add_text_child(root, "NotificationBCC", _notification_bcc);
	/* [XML] NotificationEmail Text of notification email. */
	cxml::add_text_child(root, "NotificationEmail", _notification_email);

	/* [XML] CheckForUpdates 1 to check dcpomatic.com for new versions, 0 to check only on request. */
	cxml::add_text_child(root, "CheckForUpdates", _check_for_updates ? "1" : "0");
	/* [XML] CheckForTestUpdates 1 to check dcpomatic.com for new text versions, 0 to check only on request. */
	cxml::add_text_child(root, "CheckForTestUpdates", _check_for_test_updates ? "1" : "0");

	/* [XML] MaximumJ2KVideoBitRate Maximum video bit rate (in bits per second) that can be specified in the GUI for JPEG2000 encodes. */
	cxml::add_text_child(root, "MaximumJ2KVideoBitRate", fmt::to_string(_maximum_video_bit_rate[VideoEncoding::JPEG2000]));
	/* [XML] MaximumMPEG2VideoBitRate Maximum video bit rate (in bits per second) that can be specified in the GUI for MPEG2 encodes. */
	cxml::add_text_child(root, "MaximumMPEG2VideoBitRate", fmt::to_string(_maximum_video_bit_rate[VideoEncoding::MPEG2]));
	/* [XML] AllowAnyDCPFrameRate 1 to allow users to specify any frame rate when creating DCPs, 0 to limit the GUI to standard rates. */
	cxml::add_text_child(root, "AllowAnyDCPFrameRate", _allow_any_dcp_frame_rate ? "1" : "0");
	/* [XML] AllowAnyContainer 1 to allow users to user any container ratio for their DCP, 0 to limit the GUI to DCI Flat/Scope */
	cxml::add_text_child(root, "AllowAnyContainer", _allow_any_container ? "1" : "0");
	/* [XML] Allow96kHzAudio 1 to allow users to make DCPs with 96kHz audio, 0 to always make 48kHz DCPs */
	cxml::add_text_child(root, "Allow96kHzAudio", _allow_96khz_audio ? "1" : "0");
	/* [XML] UseAllAudioChannels 1 to allow users to map audio to all 16 DCP channels, 0 to limit to the channels used in standard DCPs */
	cxml::add_text_child(root, "UseAllAudioChannels", _use_all_audio_channels ? "1" : "0");
	/* [XML] ShowExperimentalAudioProcessors 1 to offer users the (experimental) audio upmixer processors, 0 to hide them */
	cxml::add_text_child(root, "ShowExperimentalAudioProcessors", _show_experimental_audio_processors ? "1" : "0");
	/* [XML] LogTypes Types of logging to write; a bitfield where 1 is general notes, 2 warnings, 4 errors, 8 debug information related
	   to 3D, 16 debug information related to encoding, 32 debug information for timing purposes, 64 debug information related
	   to sending email, 128 debug information related to the video view, 256 information about disk writing, 512 debug information
	   related to the player, 1024 debug information related to audio analyses.
	*/
	cxml::add_text_child(root, "LogTypes", fmt::to_string(_log_types));
	/* [XML] AnalyseEBUR128 1 to do EBUR128 analyses when analysing audio, otherwise 0. */
	cxml::add_text_child(root, "AnalyseEBUR128", _analyse_ebur128 ? "1" : "0");
	/* [XML] AutomaticAudioAnalysis 1 to run audio analysis automatically when audio content is added to the film, otherwise 0. */
	cxml::add_text_child(root, "AutomaticAudioAnalysis", _automatic_audio_analysis ? "1" : "0");
#ifdef DCPOMATIC_WINDOWS
	if (_win32_console) {
		/* [XML] Win32Console 1 to open a console when running on Windows, otherwise 0.
		 * We only write this if it's true, which is a bit of a hack to allow unit tests to work
		 * more easily on Windows (without a platform-specific reference in config_write_utf8_test)
		 */
		cxml::add_text_child(root, "Win32Console", "1");
	}
#endif

	/* [XML] Signer Certificate chain and private key to use when signing DCPs and KDMs.  Should contain <code>&lt;Certificate&gt;</code>
	   tags in order and a <code>&lt;PrivateKey&gt;</code> tag all containing PEM-encoded certificates or private keys as appropriate.
	*/
	auto signer = cxml::add_child(root, "Signer");
	DCPOMATIC_ASSERT(_signer_chain);
	for (auto const& i: _signer_chain->unordered()) {
		cxml::add_text_child(signer, "Certificate", i.certificate(true));
	}
	cxml::add_text_child(signer, "PrivateKey", _signer_chain->key().get());

	/* [XML] Decryption Certificate chain and private key to use when decrypting KDMs */
	auto decryption = cxml::add_child(root, "Decryption");
	DCPOMATIC_ASSERT(_decryption_chain);
	for (auto const& i: _decryption_chain->unordered()) {
		cxml::add_text_child(decryption, "Certificate", i.certificate(true));
	}
	cxml::add_text_child(decryption, "PrivateKey", _decryption_chain->key().get());

	/* [XML] History Filename of DCP to present in the <guilabel>File</guilabel> menu of the GUI; there can be more than one
	   of these tags.
	*/
	for (auto i: _history) {
		cxml::add_text_child(root, "History", i.string());
	}

	/* [XML] PlayerHistory Filename of DCP to present in the <guilabel>File</guilabel> menu of the player; there can be more than one
	   of these tags.
	*/
	for (auto i: _player_history) {
		cxml::add_text_child(root, "PlayerHistory", i.string());
	}

	/* [XML] DKDMGroup A group of DKDMs, each with a <code>Name</code> attribute, containing other <code>&lt;DKDMGroup&gt;</code>
	   or <code>&lt;DKDM&gt;</code> tags.
	*/
	/* [XML] DKDM A DKDM as XML */
	_dkdms->as_xml(root);

	/* [XML] CinemasFile Filename of cinemas list file. */
	cxml::add_text_child(root, "CinemasFile", _cinemas_file.string());
	/* [XML] DKDMRecipientsFile Filename of DKDM recipients list file. */
	cxml::add_text_child(root, "DKDMRecipientsFile", _dkdm_recipients_file.string());
	/* [XML] ShowHintsBeforeMakeDCP 1 to show hints in the GUI before making a DCP, otherwise 0. */
	cxml::add_text_child(root, "ShowHintsBeforeMakeDCP", _show_hints_before_make_dcp ? "1" : "0");
	/* [XML] ConfirmKDMEmail 1 to confirm before sending KDM emails in the GUI, otherwise 0. */
	cxml::add_text_child(root, "ConfirmKDMEmail", _confirm_kdm_email ? "1" : "0");
	/* [XML] KDMFilenameFormat Format for KDM filenames. */
	cxml::add_text_child(root, "KDMFilenameFormat", _kdm_filename_format.specification());
	/* [XML] DKDMFilenameFormat Format for DKDM filenames. */
	cxml::add_text_child(root, "DKDMFilenameFormat", _dkdm_filename_format.specification());
	/* [XML] KDMContainerNameFormat Format for KDM containers (directories or ZIP files). */
	cxml::add_text_child(root, "KDMContainerNameFormat", _kdm_container_name_format.specification());
	/* [XML] DCPMetadataFilenameFormat Format for DCP metadata filenames. */
	cxml::add_text_child(root, "DCPMetadataFilenameFormat", _dcp_metadata_filename_format.specification());
	/* [XML] DCPAssetFilenameFormat Format for DCP asset filenames. */
	cxml::add_text_child(root, "DCPAssetFilenameFormat", _dcp_asset_filename_format.specification());
	/* [XML] JumpToSelected 1 to make the GUI jump to the start of content when it is selected, otherwise 0. */
	cxml::add_text_child(root, "JumpToSelected", _jump_to_selected ? "1" : "0");
	/* [XML] Nagged 1 if a particular nag screen has been shown and should not be shown again, otherwise 0. */
	for (int i = 0; i < NAG_COUNT; ++i) {
		auto e = cxml::add_child(root, "Nagged");
		e->set_attribute("id", fmt::to_string(i));
		e->add_child_text(_nagged[i] ? "1" : "0");
	}
	/* [XML] PreviewSound 1 to use sound in the GUI preview and player, otherwise 0. */
	cxml::add_text_child(root, "PreviewSound", _sound ? "1" : "0");
	if (_sound_output) {
		/* [XML:opt] PreviewSoundOutput Name of the audio output to use. */
		cxml::add_text_child(root, "PreviewSoundOutput", _sound_output.get());
	}
	/* [XML] CoverSheet Text of the cover sheet to write when making DCPs. */
	cxml::add_text_child(root, "CoverSheet", _cover_sheet);
	if (_last_player_load_directory) {
		cxml::add_text_child(root, "LastPlayerLoadDirectory", _last_player_load_directory->string());
	}
	/* [XML] LastKDMWriteType Last type of KDM-write: <code>flat</code> for a flat file, <code>folder</code> for a folder or <code>zip</code> for a ZIP file. */
	if (_last_kdm_write_type) {
		switch (_last_kdm_write_type.get()) {
		case KDM_WRITE_FLAT:
			cxml::add_text_child(root, "LastKDMWriteType", "flat");
			break;
		case KDM_WRITE_FOLDER:
			cxml::add_text_child(root, "LastKDMWriteType", "folder");
			break;
		case KDM_WRITE_ZIP:
			cxml::add_text_child(root, "LastKDMWriteType", "zip");
			break;
		}
	}
	/* [XML] LastDKDMWriteType Last type of DKDM-write: <code>file</code> for a file, <code>internal</code> to add to DCP-o-matic's list. */
	if (_last_dkdm_write_type) {
		switch (_last_dkdm_write_type.get()) {
		case DKDM_WRITE_INTERNAL:
			cxml::add_text_child(root, "LastDKDMWriteType", "internal");
			break;
		case DKDM_WRITE_FILE:
			cxml::add_text_child(root, "LastDKDMWriteType", "file");
			break;
		}
	}
	/* [XML] FramesInMemoryMultiplier value to multiply the encoding threads count by to get the maximum number of
	   frames to be held in memory at once.
	*/
	cxml::add_text_child(root, "FramesInMemoryMultiplier", fmt::to_string(_frames_in_memory_multiplier));

	/* [XML] DecodeReduction power of 2 to reduce DCP images by before decoding in the player. */
	if (_decode_reduction) {
		cxml::add_text_child(root, "DecodeReduction", fmt::to_string(_decode_reduction.get()));
	}

	/* [XML] DefaultNotify 1 to default jobs to notify when complete, otherwise 0. */
	cxml::add_text_child(root, "DefaultNotify", _default_notify ? "1" : "0");

	/* [XML] Notification 1 if a notification type is enabled, otherwise 0. */
	for (int i = 0; i < NOTIFICATION_COUNT; ++i) {
		auto e = cxml::add_child(root, "Notification");
		e->set_attribute("id", fmt::to_string(i));
		e->add_child_text(_notification[i] ? "1" : "0");
	}

	if (_barco_username) {
		/* [XML] BarcoUsername Username for logging into Barco's servers when downloading server certificates. */
		cxml::add_text_child(root, "BarcoUsername", *_barco_username);
	}
	if (_barco_password) {
		/* [XML] BarcoPassword Password for logging into Barco's servers when downloading server certificates. */
		cxml::add_text_child(root, "BarcoPassword", *_barco_password);
	}

	if (_christie_username) {
		/* [XML] ChristieUsername Username for logging into Christie's servers when downloading server certificates. */
		cxml::add_text_child(root, "ChristieUsername", *_christie_username);
	}
	if (_christie_password) {
		/* [XML] ChristiePassword Password for logging into Christie's servers when downloading server certificates. */
		cxml::add_text_child(root, "ChristiePassword", *_christie_password);
	}

	if (_gdc_username) {
		/* [XML] GDCUsername Username for logging into GDC's servers when downloading server certificates. */
		cxml::add_text_child(root, "GDCUsername", *_gdc_username);
	}
	if (_gdc_password) {
		/* [XML] GDCPassword Password for logging into GDC's servers when downloading server certificates. */
		cxml::add_text_child(root, "GDCPassword", *_gdc_password);
	}

	/* [XML] PlayerMode <code>window</code> for a single window, <code>full</code> for full-screen and <code>dual</code> for full screen playback
	   with separate (advanced) controls.
	*/
	switch (_player_mode) {
	case PlayerMode::WINDOW:
		cxml::add_text_child(root, "PlayerMode", "window");
		break;
	case PlayerMode::FULL:
		cxml::add_text_child(root, "PlayerMode", "full");
		break;
	case PlayerMode::DUAL:
		cxml::add_text_child(root, "PlayerMode", "dual");
		break;
	}

	if (_player_restricted_menus) {
		cxml::add_text_child(root, "PlayerRestrictedMenus", "1");
	}

	if (_playlist_editor_restricted_menus) {
		cxml::add_text_child(root, "PlaylistEditorRestrictedMenus", "1");
	}

	if (_player_crop_output_ratio) {
		cxml::add_text_child(root, "PlayerCropOutputRatio", fmt::to_string(*_player_crop_output_ratio));
	}

	/* [XML] ImageDisplay Screen number to put image on in dual-screen player mode. */
	cxml::add_text_child(root, "ImageDisplay", fmt::to_string(_image_display));
	switch (_video_view_type) {
	case VIDEO_VIEW_SIMPLE:
		cxml::add_text_child(root, "VideoViewType", "simple");
		break;
	case VIDEO_VIEW_OPENGL:
		cxml::add_text_child(root, "VideoViewType", "opengl");
		break;
	}
	/* [XML] RespectKDMValidityPeriods 1 to refuse to use KDMs that are out of date, 0 to ignore KDM dates. */
	cxml::add_text_child(root, "RespectKDMValidityPeriods", _respect_kdm_validity_periods ? "1" : "0");
	if (_player_debug_log_file) {
		/* [XML] PlayerLogFile Filename to use for player debug logs. */
		cxml::add_text_child(root, "PlayerDebugLogFile", _player_debug_log_file->string());
	}
	if (_kdm_debug_log_file) {
		/* [XML] KDMLogFile Filename to use for KDM creator debug logs. */
		cxml::add_text_child(root, "KDMDebugLogFile", _kdm_debug_log_file->string());
	}
	if (_player_content_directory) {
		/* [XML] PlayerContentDirectory Directory to use for player content in the dual-screen mode. */
		cxml::add_text_child(root, "PlayerContentDirectory", _player_content_directory->string());
	}
	if (_player_playlist_directory) {
		/* [XML] PlayerPlaylistDirectory Directory to use for player playlists in the dual-screen mode. */
		cxml::add_text_child(root, "PlayerPlaylistDirectory", _player_playlist_directory->string());
	}
	if (_player_kdm_directory) {
		/* [XML] PlayerKDMDirectory Directory to use for player KDMs in the dual-screen mode. */
		cxml::add_text_child(root, "PlayerKDMDirectory", _player_kdm_directory->string());
	}
	if (_audio_mapping) {
		_audio_mapping->as_xml(cxml::add_child(root, "AudioMapping"));
	}
	for (auto const& i: _custom_languages) {
		cxml::add_text_child(root, "CustomLanguage", i.as_string());
	}
	for (auto const& initial: _initial_paths) {
		if (initial.second) {
			cxml::add_text_child(root, initial.first, initial.second->string());
		}
	}
	cxml::add_text_child(root, "UseISDCFNameByDefault", _use_isdcf_name_by_default ? "1" : "0");
	cxml::add_text_child(root, "WriteKDMsToDisk", _write_kdms_to_disk ? "1" : "0");
	cxml::add_text_child(root, "EmailKDMs", _email_kdms ? "1" : "0");
	cxml::add_text_child(root, "DefaultKDMType", dcp::formulation_to_string(_default_kdm_type));
	cxml::add_text_child(root, "AutoCropThreshold", fmt::to_string(_auto_crop_threshold));
	if (_last_release_notes_version) {
		cxml::add_text_child(root, "LastReleaseNotesVersion", *_last_release_notes_version);
	}
	if (_main_divider_sash_position) {
		cxml::add_text_child(root, "MainDividerSashPosition", fmt::to_string(*_main_divider_sash_position));
	}
	if (_main_content_divider_sash_position) {
		cxml::add_text_child(root, "MainContentDividerSashPosition", fmt::to_string(*_main_content_divider_sash_position));
	}

	cxml::add_text_child(root, "DefaultAddFileLocation",
		_default_add_file_location == DefaultAddFileLocation::SAME_AS_LAST_TIME ? "last" : "project"
		);

	/* [XML] AllowSMPTEBv20 1 to allow the user to choose SMPTE (Bv2.0 only) as a standard, otherwise 0 */
	cxml::add_text_child(root, "AllowSMPTEBv20", _allow_smpte_bv20 ? "1" : "0");
	/* [XML] ISDCFNamePartLength Maximum length of the "name" part of an ISDCF name, which should be 14 according to the standard */
	cxml::add_text_child(root, "ISDCFNamePartLength", fmt::to_string(_isdcf_name_part_length));
	/* [XML] EnablePlayerHTTPServer 1 to enable a HTTP server to control the player, otherwise 0 */
	cxml::add_text_child(root, "EnablePlayerHTTPServer", _enable_player_http_server ? "1" : "0");
	/* [XML] PlayerHTTPServerPort Port to use for player HTTP server (if enabled) */
	cxml::add_text_child(root, "PlayerHTTPServerPort", fmt::to_string(_player_http_server_port));
	/* [XML] RelativePaths 1 to write relative paths to project metadata files, 0 to use absolute paths */
	cxml::add_text_child(root, "RelativePaths", _relative_paths ? "1" : "0");
	/* [XML] LayoutForShortScreen 1 to set up DCP-o-matic as if the screen were less than 800 pixels high */
	cxml::add_text_child(root, "LayoutForShortScreen", _layout_for_short_screen ? "1" : "0");

#ifdef DCPOMATIC_GROK
	_grok.as_xml(cxml::add_child(root, "Grok"));
#endif

	_export.write(cxml::add_child(root, "Export"));

	auto target = config_write_file();

	try {
		auto const s = doc.write_to_string_formatted();
		boost::filesystem::path tmp(string(target.string()).append(".tmp"));
		dcp::File f(tmp, "w");
		if (!f) {
			throw FileError(_("Could not open file for writing"), tmp);
		}
		f.checked_write(s.c_str(), s.bytes());
		f.close();
		dcp::filesystem::remove(target);
		dcp::filesystem::rename(tmp, target);
	} catch (xmlpp::exception& e) {
		string s = e.what();
		trim(s);
		throw FileError(s, target);
	}
}


boost::filesystem::path
Config::default_directory_or(boost::filesystem::path a) const
{
	return directory_or(_default_directory, a);
}

boost::filesystem::path
Config::default_kdm_directory_or(boost::filesystem::path a) const
{
	return directory_or(_default_kdm_directory, a);
}

boost::filesystem::path
Config::directory_or(optional<boost::filesystem::path> dir, boost::filesystem::path a) const
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
Config::drop()
{
	delete _instance;
	_instance = nullptr;
}

void
Config::changed(Property what)
{
	Changed(what);
}

void
Config::set_kdm_email_to_default()
{
	_kdm_subject = _("KDM delivery: $CPL_NAME");

	_kdm_email = variant::insert_dcpomatic(_(
		"Dear Projectionist\n\n"
		"Please find attached KDMs for $CPL_NAME.\n\n"
		"Cinema: $CINEMA_NAME\n"
		"Screen(s): $SCREENS\n\n"
		"The KDMs are valid from $START_TIME until $END_TIME.\n\n"
		"Best regards,\n{}"
		));
}

void
Config::set_notification_email_to_default()
{
	_notification_subject = variant::insert_dcpomatic(_("{} notification"));

	_notification_email = _(
		"$JOB_NAME: $JOB_STATUS"
		);
}

void
Config::reset_kdm_email()
{
	set_kdm_email_to_default();
	changed();
}

void
Config::reset_notification_email()
{
	set_notification_email_to_default();
	changed();
}

void
Config::set_cover_sheet_to_default()
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
Config::add_to_history(boost::filesystem::path p)
{
	add_to_history_internal(_history, p);
}

/** Remove non-existent items from the history */
void
Config::clean_history()
{
	clean_history_internal(_history);
}

void
Config::add_to_player_history(boost::filesystem::path p)
{
	add_to_history_internal(_player_history, p);
}

/** Remove non-existent items from the player history */
void
Config::clean_player_history()
{
	clean_history_internal(_player_history);
}

void
Config::add_to_history_internal(vector<boost::filesystem::path>& h, boost::filesystem::path p)
{
	/* Remove existing instances of this path in the history */
	h.erase(remove(h.begin(), h.end(), p), h.end());

	h.insert(h.begin(), p);
	if (h.size() > HISTORY_SIZE) {
		h.resize(HISTORY_SIZE);
	}

	changed(HISTORY);
}

void
Config::clean_history_internal(vector<boost::filesystem::path>& h)
{
	auto old = h;
	h.clear();
	for (auto i: old) {
		try {
			if (dcp::filesystem::is_directory(i)) {
				h.push_back(i);
			}
		} catch (...) {
			/* We couldn't find out if it's a directory for some reason; just ignore it */
		}
	}
}


bool
Config::have_existing(string file)
{
	return dcp::filesystem::exists(read_path(file));
}


void
Config::set_cinemas_file(boost::filesystem::path file)
{
	if (file == _cinemas_file) {
		return;
	}

	_cinemas_file = file;

	changed(CINEMAS_FILE);
}


void
Config::set_dkdm_recipients_file(boost::filesystem::path file)
{
	if (file == _dkdm_recipients_file) {
		return;
	}

	_dkdm_recipients_file = file;

	changed(OTHER);
}


void
Config::save_default_template(shared_ptr<const Film> film) const
{
	film->write_template(write_path("default.xml"));
}


void
Config::save_template(shared_ptr<const Film> film, string name) const
{
	film->write_template(template_write_path(name));
}


vector<string>
Config::templates() const
{
	if (!dcp::filesystem::exists(read_path("templates"))) {
		return {};
	}

	vector<string> n;
	for (auto const& i: dcp::filesystem::directory_iterator(read_path("templates"))) {
		n.push_back(i.path().filename().string());
	}
	return n;
}

bool
Config::existing_template(string name) const
{
	return dcp::filesystem::exists(template_read_path(name));
}


boost::filesystem::path
Config::template_read_path(string name) const
{
	return read_path("templates") / tidy_for_filename(name);
}


boost::filesystem::path
Config::default_template_read_path() const
{
	if (!boost::filesystem::exists(read_path("default.xml"))) {
		auto film = std::make_shared<const Film>(optional<boost::filesystem::path>());
		save_default_template(film);
	}

	return read_path("default.xml");
}


boost::filesystem::path
Config::template_write_path(string name) const
{
	return write_path("templates") / tidy_for_filename(name);
}


void
Config::rename_template(string old_name, string new_name) const
{
	dcp::filesystem::rename(template_read_path(old_name), template_write_path(new_name));
}

void
Config::delete_template(string name) const
{
	dcp::filesystem::remove(template_write_path(name));
}

/** @return Path to the config.xml containing the actual settings, following a link if required */
boost::filesystem::path
config_file(boost::filesystem::path main)
{
	cxml::Document f("Config");
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
Config::config_read_file()
{
	return config_file(read_path("config.xml"));
}


boost::filesystem::path
Config::config_write_file()
{
	return config_file(write_path("config.xml"));
}


void
Config::reset_cover_sheet()
{
	set_cover_sheet_to_default();
	changed();
}

void
Config::link(boost::filesystem::path new_file) const
{
	xmlpp::Document doc;
	cxml::add_text_child(doc.create_root_node("Config"), "Link", new_file.string());
	try {
		doc.write_to_file_formatted(write_path("config.xml").string());
	} catch (xmlpp::exception& e) {
		string s = e.what();
		trim(s);
		throw FileError(s, write_path("config.xml"));
	}
}

void
Config::copy_and_link(boost::filesystem::path new_file) const
{
	write();
	dcp::filesystem::copy_file(config_read_file(), new_file, dcp::filesystem::CopyOptions::OVERWRITE_EXISTING);
	link(new_file);
}

bool
Config::have_write_permission() const
{
	dcp::File f(config_write_file(), "r+");
	return static_cast<bool>(f);
}

/** @param  output_channels Number of output channels in use.
 *  @return Audio mapping for this output channel count (may be a default).
 */
AudioMapping
Config::audio_mapping(int output_channels)
{
	if (!_audio_mapping || _audio_mapping->output_channels() != output_channels) {
		/* Set up a default */
		_audio_mapping = AudioMapping(MAX_DCP_AUDIO_CHANNELS, output_channels);
		if (output_channels == 2) {
			/* Special case for stereo output.
			   Map so that Lt = L(-3dB) + Ls(-3dB) + C(-6dB) + Lfe(-10dB)
			   Rt = R(-3dB) + Rs(-3dB) + C(-6dB) + Lfe(-10dB)
			*/
			_audio_mapping->set(dcp::Channel::LEFT,   0, 1 / sqrt(2));  // L   -> Lt
			_audio_mapping->set(dcp::Channel::RIGHT,  1, 1 / sqrt(2));  // R   -> Rt
			_audio_mapping->set(dcp::Channel::CENTRE, 0, 1 / 2.0);      // C   -> Lt
			_audio_mapping->set(dcp::Channel::CENTRE, 1, 1 / 2.0);      // C   -> Rt
			_audio_mapping->set(dcp::Channel::LFE,    0, 1 / sqrt(10)); // Lfe -> Lt
			_audio_mapping->set(dcp::Channel::LFE,    1, 1 / sqrt(10)); // Lfe -> Rt
			_audio_mapping->set(dcp::Channel::LS,     0, 1 / sqrt(2));  // Ls  -> Lt
			_audio_mapping->set(dcp::Channel::RS,     1, 1 / sqrt(2));  // Rs  -> Rt
		} else {
			/* 1:1 mapping */
			for (int i = 0; i < min(MAX_DCP_AUDIO_CHANNELS, output_channels); ++i) {
				_audio_mapping->set(i, i, 1);
			}
		}
	}

	return *_audio_mapping;
}

void
Config::set_audio_mapping(AudioMapping m)
{
	_audio_mapping = m;
	changed(AUDIO_MAPPING);
}

void
Config::set_audio_mapping_to_default()
{
	DCPOMATIC_ASSERT(_audio_mapping);
	auto const ch = _audio_mapping->output_channels();
	_audio_mapping = boost::none;
	_audio_mapping = audio_mapping(ch);
	changed(AUDIO_MAPPING);
}


void
Config::add_custom_language(dcp::LanguageTag tag)
{
	if (find(_custom_languages.begin(), _custom_languages.end(), tag) == _custom_languages.end()) {
		_custom_languages.push_back(tag);
		changed();
	}
}


optional<Config::BadReason>
Config::check_certificates() const
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
save_all_config_as_zip(boost::filesystem::path zip_file)
{
	Zipper zipper(zip_file);

	auto config = Config::instance();
	zipper.add("config.xml", dcp::file_to_string(config->config_read_file()));
	if (dcp::filesystem::exists(config->cinemas_file())) {
		zipper.add("cinemas.sqlite3", dcp::file_to_string(config->cinemas_file()));
	}
	if (dcp::filesystem::exists(config->dkdm_recipients_file())) {
		zipper.add("dkdm_recipients.sqlite3", dcp::file_to_string(config->dkdm_recipients_file()));
	}

	zipper.close();
}


void
Config::load_from_zip(boost::filesystem::path zip_file, CinemasAction action)
{
	backup();

	auto const current_cinemas = cinemas_file();
	/* This is (unfortunately) a full path, and the user can't change it, so
	 * we always want to use that same path in the future no matter what is in the
	 * config.xml that we are about to load.
	 */
	auto const current_dkdm_recipients = dkdm_recipients_file();

	Unzipper unzipper(zip_file);
	dcp::write_string_to_file(unzipper.get("config.xml"), config_write_file());

	if (action == CinemasAction::WRITE_TO_PATH_IN_ZIPPED_CONFIG) {
		/* Read the zipped config, so that the cinemas file path is the new one and
		 * we write the cinemas to it.
		 */
		read();
		boost::filesystem::create_directories(cinemas_file().parent_path());
		set_dkdm_recipients_file(current_dkdm_recipients);
	}

	if (unzipper.contains("cinemas.xml") && action != CinemasAction::IGNORE) {
		CinemaList cinemas;
		cinemas.clear();
		cinemas.read_legacy_string(unzipper.get("cinemas.xml"));
	}

	if (unzipper.contains("dkdm_recipients.xml")) {
		DKDMRecipientList recipients;
		recipients.clear();
		recipients.read_legacy_string(unzipper.get("dkdm_recipients.xml"));
	}

	if (unzipper.contains("cinemas.sqlite3") && action != CinemasAction::IGNORE) {
		dcp::write_string_to_file(unzipper.get("cinemas.sqlite3"), cinemas_file());
	}

	if (unzipper.contains("dkdm_recipients.sqlite3")) {
		dcp::write_string_to_file(unzipper.get("dkdm_recipients.sqlite3"), dkdm_recipients_file());
	}

	if (action != CinemasAction::WRITE_TO_PATH_IN_ZIPPED_CONFIG) {
		/* Read the zipped config, then reset the cinemas file to be the old one */
		read();
		set_cinemas_file(current_cinemas);
		set_dkdm_recipients_file(current_dkdm_recipients);
	}

	changed(Property::USE_ANY_SERVERS);
	changed(Property::SERVERS);
	changed(Property::SOUND);
	changed(Property::SOUND_OUTPUT);
	changed(Property::PLAYER_CONTENT_DIRECTORY);
	changed(Property::PLAYER_PLAYLIST_DIRECTORY);
	changed(Property::PLAYER_DEBUG_LOG);
	changed(Property::HISTORY);
	changed(Property::SHOW_EXPERIMENTAL_AUDIO_PROCESSORS);
	changed(Property::AUDIO_MAPPING);
	changed(Property::AUTO_CROP_THRESHOLD);
	changed(Property::ALLOW_SMPTE_BV20);
	changed(Property::ISDCF_NAME_PART_LENGTH);
	changed(Property::CINEMAS_FILE);
	changed(Property::OTHER);
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
	if (iter == _initial_paths.end()) {
		return {};
	}
	return iter->second;
}


bool
Config::zip_contains_cinemas(boost::filesystem::path zip)
{
	Unzipper unzipper(zip);
	return unzipper.contains("cinemas.sqlite3") || unzipper.contains("cinemas.xml");
}


boost::filesystem::path
Config::cinemas_file_from_zip(boost::filesystem::path zip)
{
	Unzipper unzipper(zip);
	DCPOMATIC_ASSERT(unzipper.contains("config.xml"));
	cxml::Document document("Config");
	document.read_string(unzipper.get("config.xml"));
	return document.string_child("CinemasFile");
}


boost::filesystem::path
Config::cinemas_file() const
{
	if (_cinemas_file.is_absolute()) {
		return _cinemas_file;
	}

	return read_path("config.xml").parent_path() / _cinemas_file;
}


#ifdef DCPOMATIC_GROK

Config::Grok::Grok()
	: licence_server(default_grok_licence_server)
{}


Config::Grok::Grok(cxml::ConstNodePtr node)
	: enable(node->bool_child("Enable"))
	, binary_location(node->string_child("BinaryLocation"))
	, selected(node->number_child<int>("Selected"))
	, licence_server(node->string_child("LicenceServer"))
	, licence(node->string_child("Licence"))
{
	if (licence_server.empty()) {
		licence_server = default_grok_licence_server;
	}
}


void
Config::Grok::as_xml(xmlpp::Element* node) const
{
	node->add_child("BinaryLocation")->add_child_text(binary_location.string());
	node->add_child("Enable")->add_child_text((enable ? "1" : "0"));
	node->add_child("Selected")->add_child_text(fmt::to_string(selected));
	node->add_child("LicenceServer")->add_child_text(licence_server);
	node->add_child("Licence")->add_child_text(licence);
}


void
Config::set_grok(Grok const& grok)
{
	_grok = grok;
	changed(GROK);
}

#endif
