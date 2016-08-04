/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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
#include "raw_convert.h"
#include <dcp/name_format.h>
#include <dcp/colour_matrix.h>
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
using boost::algorithm::trim;

Config* Config::_instance = 0;
boost::signals2::signal<void ()> Config::FailedToLoad;

/** Construct default configuration */
Config::Config ()
{
	set_defaults ();
}

void
Config::set_defaults ()
{
	_num_local_encoding_threads = max (2U, boost::thread::hardware_concurrency ());
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
	_default_dcp_content_type = DCPContentType::from_isdcf_name ("FTR");
	_default_dcp_audio_channels = 6;
	_default_j2k_bandwidth = 100000000;
	_default_audio_delay = 0;
	_default_interop = false;
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
	_kdm_filename_format = dcp::NameFormat ("KDM %f %c %s");
	_dcp_metadata_filename_format = dcp::NameFormat ("%t_%i");
	_dcp_asset_filename_format = dcp::NameFormat ("%t_%i");

	_allowed_dcp_frame_rates.clear ();
	_allowed_dcp_frame_rates.push_back (24);
	_allowed_dcp_frame_rates.push_back (25);
	_allowed_dcp_frame_rates.push_back (30);
	_allowed_dcp_frame_rates.push_back (48);
	_allowed_dcp_frame_rates.push_back (50);
	_allowed_dcp_frame_rates.push_back (60);

	set_kdm_email_to_default ();
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
	f.read_file (path ("config.xml"));
	optional<string> c;

	optional<int> version = f.optional_number_child<int> ("Version");

	_num_local_encoding_threads = f.number_child<int> ("NumLocalEncodingThreads");
	_default_directory = f.string_child ("DefaultDirectory");

	boost::optional<int> b = f.optional_number_child<int> ("ServerPort");
	if (!b) {
		b = f.optional_number_child<int> ("ServerPortBase");
	}
	_server_port_base = b.get ();

	boost::optional<bool> u = f.optional_bool_child ("UseAnyServers");
	_use_any_servers = u.get_value_or (true);

	list<cxml::NodePtr> servers = f.node_children ("Server");
	for (list<cxml::NodePtr>::iterator i = servers.begin(); i != servers.end(); ++i) {
		if ((*i)->node_children("HostName").size() == 1) {
			_servers.push_back ((*i)->string_child ("HostName"));
		} else {
			_servers.push_back ((*i)->content ());
		}
	}

	_only_servers_encode = f.optional_bool_child ("OnlyServersEncode").get_value_or (false);
	_tms_protocol = static_cast<Protocol> (f.optional_number_child<int> ("TMSProtocol").get_value_or (static_cast<int> (PROTOCOL_SCP)));
	_tms_ip = f.string_child ("TMSIP");
	_tms_path = f.string_child ("TMSPath");
	_tms_user = f.string_child ("TMSUser");
	_tms_password = f.string_child ("TMSPassword");

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
	for (list<cxml::NodePtr>::const_iterator i = his.begin(); i != his.end(); ++i) {
		_history.push_back ((*i)->content ());
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

	list<cxml::NodePtr> dkdm = f.node_children ("DKDM");
	BOOST_FOREACH (cxml::NodePtr i, f.node_children ("DKDM")) {
		_dkdms.push_back (dcp::EncryptedKDM (i->content ()));
	}

	_cinemas_file = f.optional_string_child("CinemasFile").get_value_or (path ("cinemas.xml").string ());
	_show_hints_before_make_dcp = f.optional_bool_child("ShowHintsBeforeMakeDCP").get_value_or (true);
	_kdm_filename_format = dcp::NameFormat (f.optional_string_child("KDMFilenameFormat").get_value_or ("KDM %f %c %s"));
	_dcp_metadata_filename_format = dcp::NameFormat (f.optional_string_child("DCPMetadataFilenameFormat").get_value_or ("%t_%i"));
	_dcp_asset_filename_format = dcp::NameFormat (f.optional_string_child("DCPAssetFilenameFormat").get_value_or ("%t_%i"));

	/* Replace any cinemas from config.xml with those from the configured file */
	if (boost::filesystem::exists (_cinemas_file)) {
		cxml::Document f ("Cinemas");
		f.read_file (_cinemas_file);
		read_cinemas (f);
	}
}
catch (...) {
	if (have_existing ("config.xml")) {
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
	write_config_xml ();
	write_cinemas_xml ();
}

void
Config::write_config_xml () const
{
	xmlpp::Document doc;
	xmlpp::Element* root = doc.create_root_node ("Config");

	root->add_child("Version")->add_child_text ("2");
	root->add_child("NumLocalEncodingThreads")->add_child_text (raw_convert<string> (_num_local_encoding_threads));
	root->add_child("DefaultDirectory")->add_child_text (_default_directory.string ());
	root->add_child("ServerPortBase")->add_child_text (raw_convert<string> (_server_port_base));
	root->add_child("UseAnyServers")->add_child_text (_use_any_servers ? "1" : "0");

	for (vector<string>::const_iterator i = _servers.begin(); i != _servers.end(); ++i) {
		root->add_child("Server")->add_child_text (*i);
	}

	root->add_child("OnlyServersEncode")->add_child_text (_only_servers_encode ? "1" : "0");
	root->add_child("TMSProtocol")->add_child_text (raw_convert<string> (_tms_protocol));
	root->add_child("TMSIP")->add_child_text (_tms_ip);
	root->add_child("TMSPath")->add_child_text (_tms_path);
	root->add_child("TMSUser")->add_child_text (_tms_user);
	root->add_child("TMSPassword")->add_child_text (_tms_password);
	if (_cinema_sound_processor) {
		root->add_child("CinemaSoundProcessor")->add_child_text (_cinema_sound_processor->id ());
	}
	if (_language) {
		root->add_child("Language")->add_child_text (_language.get());
	}
	if (_default_container) {
		root->add_child("DefaultContainer")->add_child_text (_default_container->id ());
	}
	if (_default_dcp_content_type) {
		root->add_child("DefaultDCPContentType")->add_child_text (_default_dcp_content_type->isdcf_name ());
	}
	root->add_child("DefaultDCPAudioChannels")->add_child_text (raw_convert<string> (_default_dcp_audio_channels));
	root->add_child("DCPIssuer")->add_child_text (_dcp_issuer);
	root->add_child("DCPCreator")->add_child_text (_dcp_creator);

	_default_isdcf_metadata.as_xml (root->add_child ("ISDCFMetadata"));

	root->add_child("DefaultStillLength")->add_child_text (raw_convert<string> (_default_still_length));
	root->add_child("DefaultJ2KBandwidth")->add_child_text (raw_convert<string> (_default_j2k_bandwidth));
	root->add_child("DefaultAudioDelay")->add_child_text (raw_convert<string> (_default_audio_delay));
	root->add_child("DefaultInterop")->add_child_text (_default_interop ? "1" : "0");
	root->add_child("MailServer")->add_child_text (_mail_server);
	root->add_child("MailPort")->add_child_text (raw_convert<string> (_mail_port));
	root->add_child("MailUser")->add_child_text (_mail_user);
	root->add_child("MailPassword")->add_child_text (_mail_password);
	root->add_child("KDMSubject")->add_child_text (_kdm_subject);
	root->add_child("KDMFrom")->add_child_text (_kdm_from);
	BOOST_FOREACH (string i, _kdm_cc) {
		root->add_child("KDMCC")->add_child_text (i);
	}
	root->add_child("KDMBCC")->add_child_text (_kdm_bcc);
	root->add_child("KDMEmail")->add_child_text (_kdm_email);

	root->add_child("CheckForUpdates")->add_child_text (_check_for_updates ? "1" : "0");
	root->add_child("CheckForTestUpdates")->add_child_text (_check_for_test_updates ? "1" : "0");

	root->add_child("MaximumJ2KBandwidth")->add_child_text (raw_convert<string> (_maximum_j2k_bandwidth));
	root->add_child("AllowAnyDCPFrameRate")->add_child_text (_allow_any_dcp_frame_rate ? "1" : "0");
	root->add_child("LogTypes")->add_child_text (raw_convert<string> (_log_types));
	root->add_child("AnalyseEBUR128")->add_child_text (_analyse_ebur128 ? "1" : "0");
	root->add_child("AutomaticAudioAnalysis")->add_child_text (_automatic_audio_analysis ? "1" : "0");
#ifdef DCPOMATIC_WINDOWS
	root->add_child("Win32Console")->add_child_text (_win32_console ? "1" : "0");
#endif

	xmlpp::Element* signer = root->add_child ("Signer");
	DCPOMATIC_ASSERT (_signer_chain);
	BOOST_FOREACH (dcp::Certificate const & i, _signer_chain->root_to_leaf ()) {
		signer->add_child("Certificate")->add_child_text (i.certificate (true));
	}
	signer->add_child("PrivateKey")->add_child_text (_signer_chain->key().get ());

	xmlpp::Element* decryption = root->add_child ("Decryption");
	DCPOMATIC_ASSERT (_decryption_chain);
	BOOST_FOREACH (dcp::Certificate const & i, _decryption_chain->root_to_leaf ()) {
		decryption->add_child("Certificate")->add_child_text (i.certificate (true));
	}
	decryption->add_child("PrivateKey")->add_child_text (_decryption_chain->key().get ());

	for (vector<boost::filesystem::path>::const_iterator i = _history.begin(); i != _history.end(); ++i) {
		root->add_child("History")->add_child_text (i->string ());
	}

	BOOST_FOREACH (dcp::EncryptedKDM i, _dkdms) {
		root->add_child("DKDM")->add_child_text (i.as_xml ());
	}

	root->add_child("CinemasFile")->add_child_text (_cinemas_file.string());
	root->add_child("ShowHintsBeforeMakeDCP")->add_child_text (_show_hints_before_make_dcp ? "1" : "0");
	root->add_child("KDMFilenameFormat")->add_child_text (_kdm_filename_format.specification ());
	root->add_child("DCPMetadataFilenameFormat")->add_child_text (_dcp_metadata_filename_format.specification ());
	root->add_child("DCPAssetFilenameFormat")->add_child_text (_dcp_asset_filename_format.specification ());

	try {
		doc.write_to_file_formatted (path("config.xml").string ());
	} catch (xmlpp::exception& e) {
		string s = e.what ();
		trim (s);
		throw FileError (s, path("config.xml"));
	}
}

void
Config::write_cinemas_xml () const
{
	xmlpp::Document doc;
	xmlpp::Element* root = doc.create_root_node ("Cinemas");
	root->add_child("Version")->add_child_text ("1");

	for (list<shared_ptr<Cinema> >::const_iterator i = _cinemas.begin(); i != _cinemas.end(); ++i) {
		(*i)->as_xml (root->add_child ("Cinema"));
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
	if (_default_directory.empty()) {
		return a;
	}

	boost::system::error_code ec;
	bool const e = boost::filesystem::exists (_default_directory, ec);
	if (ec || !e) {
		return a;
	}

	return _default_directory;
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
	for (list<cxml::NodePtr>::iterator i = cin.begin(); i != cin.end(); ++i) {
		/* Slightly grotty two-part construction of Cinema here so that we can use
		   shared_from_this.
		*/
		shared_ptr<Cinema> cinema (new Cinema (*i));
		cinema->read_screens (*i);
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
