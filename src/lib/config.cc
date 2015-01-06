/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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

#include "config.h"
#include "server.h"
#include "scaler.h"
#include "filter.h"
#include "ratio.h"
#include "dcp_content_type.h"
#include "cinema_sound_processor.h"
#include "colour_conversion.h"
#include "cinema.h"
#include "util.h"
#include "cross.h"
#include <dcp/colour_matrix.h>
#include <dcp/raw_convert.h>
#include <dcp/signer.h>
#include <dcp/certificate_chain.h>
#include <libcxml/cxml.h>
#include <glib.h>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <cstdlib>
#include <fstream>

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
using dcp::raw_convert;

Config* Config::_instance = 0;

/** Construct default configuration */
Config::Config ()
	: _num_local_encoding_threads (max (2U, boost::thread::hardware_concurrency()))
	, _server_port_base (6192)
	, _use_any_servers (true)
	, _tms_path (".")
	, _cinema_sound_processor (CinemaSoundProcessor::from_id (N_("dolby_cp750")))
	, _allow_any_dcp_frame_rate (false)
	, _default_still_length (10)
	, _default_container (Ratio::from_id ("185"))
	, _default_dcp_content_type (DCPContentType::from_isdcf_name ("FTR"))
	, _default_j2k_bandwidth (100000000)
	, _default_audio_delay (0)
	, _check_for_updates (false)
	, _check_for_test_updates (false)
	, _maximum_j2k_bandwidth (250000000)
	, _log_types (Log::TYPE_GENERAL | Log::TYPE_WARNING | Log::TYPE_ERROR | Log::TYPE_DEBUG)
#ifdef DCPOMATIC_WINDOWS	  
	, _win32_console (false)
#endif	  
{
	_allowed_dcp_frame_rates.push_back (24);
	_allowed_dcp_frame_rates.push_back (25);
	_allowed_dcp_frame_rates.push_back (30);
	_allowed_dcp_frame_rates.push_back (48);
	_allowed_dcp_frame_rates.push_back (50);
	_allowed_dcp_frame_rates.push_back (60);

	_colour_conversions.push_back (PresetColourConversion (_("sRGB"), dcp::ColourConversion::srgb_to_xyz ()));
	_colour_conversions.push_back (PresetColourConversion (_("Rec. 709"), dcp::ColourConversion::rec709_to_xyz ()));

	reset_kdm_email ();
}

void
Config::read ()
{
	if (!boost::filesystem::exists (file ())) {
		/* Make a new set of signing certificates and key */
		_signer.reset (new dcp::Signer (openssl_path ()));
		/* And decryption keys */
		make_decryption_keys ();
		return;
	}

	cxml::Document f ("Config");
	f.read_file (file ());
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

	if (f.optional_string_child ("DCPMetadataIssuer")) {
		_dcp_issuer = f.string_child ("DCPMetadataIssuer");
	} else if (f.optional_string_child ("DCPIssuer")) {
		_dcp_issuer = f.string_child ("DCPIssuer");
	}
	
	if (version && version.get() >= 2) {
		_default_isdcf_metadata = ISDCFMetadata (f.node_child ("ISDCFMetadata"));
	} else {
		_default_isdcf_metadata = ISDCFMetadata (f.node_child ("DCIMetadata"));
	}
	
	_default_still_length = f.optional_number_child<int>("DefaultStillLength").get_value_or (10);
	_default_j2k_bandwidth = f.optional_number_child<int>("DefaultJ2KBandwidth").get_value_or (200000000);
	_default_audio_delay = f.optional_number_child<int>("DefaultAudioDelay").get_value_or (0);

	list<cxml::NodePtr> cc = f.node_children ("ColourConversion");

	if (!cc.empty ()) {
		_colour_conversions.clear ();
	}

	try {
		for (list<cxml::NodePtr>::iterator i = cc.begin(); i != cc.end(); ++i) {
			/* This is a bit of a hack; use 32 (the first Film state file version for the 2.x branch)
			   for version 2 and 10 (the current Film state version for the 1.x branch) for version 1.
			*/
			_colour_conversions.push_back (PresetColourConversion (*i, version == 2 ? 32 : 10));
		}
	} catch (cxml::Error) {
		/* Probably failed to load an old-style ColourConversion tag; just give up */
		_colour_conversions.push_back (PresetColourConversion (_("sRGB"), dcp::ColourConversion::srgb_to_xyz ()));
		_colour_conversions.push_back (PresetColourConversion (_("Rec. 709"), dcp::ColourConversion::rec709_to_xyz ()));
	}

	list<cxml::NodePtr> cin = f.node_children ("Cinema");
	for (list<cxml::NodePtr>::iterator i = cin.begin(); i != cin.end(); ++i) {
		/* Slightly grotty two-part construction of Cinema here so that we can use
		   shared_from_this.
		*/
		shared_ptr<Cinema> cinema (new Cinema (*i));
		cinema->read_screens (*i);
		_cinemas.push_back (cinema);
	}

	_mail_server = f.string_child ("MailServer");
	_mail_user = f.optional_string_child("MailUser").get_value_or ("");
	_mail_password = f.optional_string_child("MailPassword").get_value_or ("");
	_kdm_subject = f.optional_string_child ("KDMSubject").get_value_or (_("KDM delivery: $CPL_NAME"));
	_kdm_from = f.string_child ("KDMFrom");
	_kdm_cc = f.optional_string_child ("KDMCC").get_value_or ("");
	_kdm_bcc = f.optional_string_child ("KDMBCC").get_value_or ("");
	_kdm_email = f.string_child ("KDMEmail");

	_check_for_updates = f.optional_bool_child("CheckForUpdates").get_value_or (false);
	_check_for_test_updates = f.optional_bool_child("CheckForTestUpdates").get_value_or (false);

	_maximum_j2k_bandwidth = f.optional_number_child<int> ("MaximumJ2KBandwidth").get_value_or (250000000);
	_allow_any_dcp_frame_rate = f.optional_bool_child ("AllowAnyDCPFrameRate");

	_log_types = f.optional_number_child<int> ("LogTypes").get_value_or (Log::TYPE_GENERAL | Log::TYPE_WARNING | Log::TYPE_ERROR);
#ifdef DCPOMATIC_WINDOWS	
	_win32_console = f.optional_bool_child ("Win32Console").get_value_or (false);
#endif	

	list<cxml::NodePtr> his = f.node_children ("History");
	for (list<cxml::NodePtr>::const_iterator i = his.begin(); i != his.end(); ++i) {
		_history.push_back ((*i)->content ());
	}

	cxml::NodePtr signer = f.optional_node_child ("Signer");
	dcp::CertificateChain signer_chain;
	if (signer) {
		/* Read the signing certificates and private key in from the config file */
		list<cxml::NodePtr> certificates = signer->node_children ("Certificate");
		for (list<cxml::NodePtr>::const_iterator i = certificates.begin(); i != certificates.end(); ++i) {
			signer_chain.add (dcp::Certificate ((*i)->content ()));
		}

		_signer.reset (new dcp::Signer (signer_chain, signer->string_child ("PrivateKey")));
	} else {
		/* Make a new set of signing certificates and key */
		_signer.reset (new dcp::Signer (openssl_path ()));
	}

	if (f.optional_string_child ("DecryptionCertificate")) {
		_decryption_certificate = dcp::Certificate (f.string_child ("DecryptionCertificate"));
	}

	if (f.optional_string_child ("DecryptionPrivateKey")) {
		_decryption_private_key = f.string_child ("DecryptionPrivateKey");
	}

	if (!f.optional_string_child ("DecryptionCertificate") || !f.optional_string_child ("DecryptionPrivateKey")) {
		/* Generate our own decryption certificate and key if either is not present in config */
		make_decryption_keys ();
	}
}

void
Config::make_decryption_keys ()
{
	boost::filesystem::path p = dcp::make_certificate_chain (openssl_path ());
	_decryption_certificate = dcp::Certificate (dcp::file_to_string (p / "leaf.signed.pem"));
	_decryption_private_key = dcp::file_to_string (p / "leaf.key");
	boost::filesystem::remove_all (p);
}

/** @return Filename to write configuration to */
boost::filesystem::path
Config::file () const
{
	boost::filesystem::path p;
	p /= g_get_user_config_dir ();
	boost::system::error_code ec;
	boost::filesystem::create_directory (p, ec);
	p /= "dcpomatic2";
	boost::filesystem::create_directory (p, ec);
	p /= "config.xml";
	return p;
}

/** @return Singleton instance */
Config *
Config::instance ()
{
	if (_instance == 0) {
		_instance = new Config;
		try {
			_instance->read ();
		} catch (exception& e) {
			/* configuration load failed; never mind, just
			   stick with the default.
			*/
			cerr << "dcpomatic: failed to load configuration (" << e.what() << ")\n";
		} catch (...) {
			cerr << "dcpomatic: failed to load configuration\n";
		}
	}

	return _instance;
}

/** Write our configuration to disk */
void
Config::write () const
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
	root->add_child("DCPIssuer")->add_child_text (_dcp_issuer);

	_default_isdcf_metadata.as_xml (root->add_child ("ISDCFMetadata"));

	root->add_child("DefaultStillLength")->add_child_text (raw_convert<string> (_default_still_length));
	root->add_child("DefaultJ2KBandwidth")->add_child_text (raw_convert<string> (_default_j2k_bandwidth));
	root->add_child("DefaultAudioDelay")->add_child_text (raw_convert<string> (_default_audio_delay));

	for (vector<PresetColourConversion>::const_iterator i = _colour_conversions.begin(); i != _colour_conversions.end(); ++i) {
		i->as_xml (root->add_child ("ColourConversion"));
	}

	for (list<shared_ptr<Cinema> >::const_iterator i = _cinemas.begin(); i != _cinemas.end(); ++i) {
		(*i)->as_xml (root->add_child ("Cinema"));
	}

	root->add_child("MailServer")->add_child_text (_mail_server);
	root->add_child("MailUser")->add_child_text (_mail_user);
	root->add_child("MailPassword")->add_child_text (_mail_password);
	root->add_child("KDMSubject")->add_child_text (_kdm_subject);
	root->add_child("KDMFrom")->add_child_text (_kdm_from);
	root->add_child("KDMCC")->add_child_text (_kdm_cc);
	root->add_child("KDMBCC")->add_child_text (_kdm_bcc);
	root->add_child("KDMEmail")->add_child_text (_kdm_email);

	root->add_child("CheckForUpdates")->add_child_text (_check_for_updates ? "1" : "0");
	root->add_child("CheckForTestUpdates")->add_child_text (_check_for_test_updates ? "1" : "0");

	root->add_child("MaximumJ2KBandwidth")->add_child_text (raw_convert<string> (_maximum_j2k_bandwidth));
	root->add_child("AllowAnyDCPFrameRate")->add_child_text (_allow_any_dcp_frame_rate ? "1" : "0");
	root->add_child("LogTypes")->add_child_text (raw_convert<string> (_log_types));
#ifdef DCPOMATIC_WINDOWS	
	root->add_child("Win32Console")->add_child_text (_win32_console ? "1" : "0");
#endif	

	xmlpp::Element* signer = root->add_child ("Signer");
	dcp::CertificateChain::List certs = _signer->certificates().root_to_leaf ();
	for (dcp::CertificateChain::List::const_iterator i = certs.begin(); i != certs.end(); ++i) {
		signer->add_child("Certificate")->add_child_text (i->certificate (true));
	}
	signer->add_child("PrivateKey")->add_child_text (_signer->key ());

	root->add_child("DecryptionCertificate")->add_child_text (_decryption_certificate.certificate (true));
	root->add_child("DecryptionPrivateKey")->add_child_text (_decryption_private_key);

	for (vector<boost::filesystem::path>::const_iterator i = _history.begin(); i != _history.end(); ++i) {
		root->add_child("History")->add_child_text (i->string ());
	}

	try {
		doc.write_to_file_formatted (file().string ());
	} catch (xmlpp::exception& e) {
		string s = e.what ();
		trim (s);
		throw FileError (s, file ());
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
Config::changed ()
{
	write ();
	Changed ();
}

void
Config::reset_kdm_email ()
{
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
