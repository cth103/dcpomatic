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

#include <sstream>
#include <cstdlib>
#include <fstream>
#include <glib.h>
#include <boost/filesystem.hpp>
#include <libdcp/colour_matrix.h>
#include <libcxml/cxml.h>
#include "config.h"
#include "server.h"
#include "scaler.h"
#include "filter.h"
#include "ratio.h"
#include "dcp_content_type.h"
#include "sound_processor.h"
#include "colour_conversion.h"
#include "cinema.h"
#include "util.h"

#include "i18n.h"

using std::vector;
using std::ifstream;
using std::string;
using std::ofstream;
using std::list;
using std::max;
using std::exception;
using std::cerr;
using boost::shared_ptr;
using boost::lexical_cast;
using boost::optional;

Config* Config::_instance = 0;

/** Construct default configuration */
Config::Config ()
	: _num_local_encoding_threads (max (2U, boost::thread::hardware_concurrency()))
	, _server_port (6192)
	, _tms_path (".")
	, _sound_processor (SoundProcessor::from_id (N_("dolby_cp750")))
	, _default_still_length (10)
	, _default_container (Ratio::from_id ("185"))
	, _default_dcp_content_type (DCPContentType::from_dci_name ("TST"))
	, _default_j2k_bandwidth (200000000)
	, _kdm_email (
		"Dear Projectionist\n\nPlease find attached KDMs for $CPL_NAME.\n\nBest regards,\nDCP-o-matic"
		)
{
	_allowed_dcp_frame_rates.push_back (24);
	_allowed_dcp_frame_rates.push_back (25);
	_allowed_dcp_frame_rates.push_back (30);
	_allowed_dcp_frame_rates.push_back (48);
	_allowed_dcp_frame_rates.push_back (50);
	_allowed_dcp_frame_rates.push_back (60);

	_colour_conversions.push_back (PresetColourConversion (_("sRGB"), 2.4, true, libdcp::colour_matrix::srgb_to_xyz, 2.6));
	_colour_conversions.push_back (PresetColourConversion (_("sRGB non-linearised"), 2.4, false, libdcp::colour_matrix::srgb_to_xyz, 2.6));
	_colour_conversions.push_back (PresetColourConversion (_("Rec. 709"), 2.2, false, libdcp::colour_matrix::rec709_to_xyz, 2.6));
}

void
Config::read ()
{
	LocaleGuard lg;
	
	if (!boost::filesystem::exists (file (false))) {
		read_old_metadata ();
		return;
	}

	cxml::Document f ("Config");
	f.read_file (file (false));
	optional<string> c;

	optional<int> version = f.optional_number_child<int> ("Version");

	_num_local_encoding_threads = f.number_child<int> ("NumLocalEncodingThreads");
	_default_directory = f.string_child ("DefaultDirectory");
	_server_port = f.number_child<int> ("ServerPort");
	
	list<shared_ptr<cxml::Node> > servers = f.node_children ("Server");
	for (list<shared_ptr<cxml::Node> >::iterator i = servers.begin(); i != servers.end(); ++i) {
		_servers.push_back (ServerDescription (*i));
	}

	_tms_ip = f.string_child ("TMSIP");
	_tms_path = f.string_child ("TMSPath");
	_tms_user = f.string_child ("TMSUser");
	_tms_password = f.string_child ("TMSPassword");

	c = f.optional_string_child ("SoundProcessor");
	if (c) {
		_sound_processor = SoundProcessor::from_id (c.get ());
	}

	_language = f.optional_string_child ("Language");

	c = f.optional_string_child ("DefaultContainer");
	if (c) {
		_default_container = Ratio::from_id (c.get ());
	}

	c = f.optional_string_child ("DefaultDCPContentType");
	if (c) {
		_default_dcp_content_type = DCPContentType::from_dci_name (c.get ());
	}

	_dcp_metadata.issuer = f.optional_string_child ("DCPMetadataIssuer").get_value_or ("");
	_dcp_metadata.creator = f.optional_string_child ("DCPMetadataCreator").get_value_or ("");

	_default_dci_metadata = DCIMetadata (f.node_child ("DCIMetadata"));
	_default_still_length = f.optional_number_child<int>("DefaultStillLength").get_value_or (10);
	_default_j2k_bandwidth = f.optional_number_child<int>("DefaultJ2KBandwidth").get_value_or (200000000);

	list<shared_ptr<cxml::Node> > cc = f.node_children ("ColourConversion");

	if (!cc.empty ()) {
		_colour_conversions.clear ();
	}
	
	for (list<shared_ptr<cxml::Node> >::iterator i = cc.begin(); i != cc.end(); ++i) {
		_colour_conversions.push_back (PresetColourConversion (*i));
	}

	if (!version) {
		/* Loading version 0 (before Rec. 709 was added as a preset).
		   Add it in.
		*/
		_colour_conversions.push_back (PresetColourConversion (_("Rec. 709"), 2.2, false, libdcp::colour_matrix::rec709_to_xyz, 2.6));
	}

	list<shared_ptr<cxml::Node> > cin = f.node_children ("Cinema");
	for (list<shared_ptr<cxml::Node> >::iterator i = cin.begin(); i != cin.end(); ++i) {
		/* Slightly grotty two-part construction of Cinema here so that we can use
		   shared_from_this.
		*/
		shared_ptr<Cinema> cinema (new Cinema (*i));
		cinema->read_screens (*i);
		_cinemas.push_back (cinema);
	}

	_mail_server = f.string_child ("MailServer");
	_kdm_from = f.string_child ("KDMFrom");
	_kdm_email = f.string_child ("KDMEmail");
}

void
Config::read_old_metadata ()
{
	ifstream f (file(true).string().c_str ());
	string line;

	while (getline (f, line)) {
		if (line.empty ()) {
			continue;
		}

		if (line[0] == '#') {
			continue;
		}

		size_t const s = line.find (' ');
		if (s == string::npos) {
			continue;
		}
		
		string const k = line.substr (0, s);
		string const v = line.substr (s + 1);

		if (k == N_("num_local_encoding_threads")) {
			_num_local_encoding_threads = atoi (v.c_str ());
		} else if (k == N_("default_directory")) {
			_default_directory = v;
		} else if (k == N_("server_port")) {
			_server_port = atoi (v.c_str ());
		} else if (k == N_("server")) {
			optional<ServerDescription> server = ServerDescription::create_from_metadata (v);
			if (server) {
				_servers.push_back (server.get ());
			}
		} else if (k == N_("tms_ip")) {
			_tms_ip = v;
		} else if (k == N_("tms_path")) {
			_tms_path = v;
		} else if (k == N_("tms_user")) {
			_tms_user = v;
		} else if (k == N_("tms_password")) {
			_tms_password = v;
		} else if (k == N_("sound_processor")) {
			_sound_processor = SoundProcessor::from_id (v);
		} else if (k == "language") {
			_language = v;
		} else if (k == "default_container") {
			_default_container = Ratio::from_id (v);
		} else if (k == "default_dcp_content_type") {
			_default_dcp_content_type = DCPContentType::from_dci_name (v);
		} else if (k == "dcp_metadata_issuer") {
			_dcp_metadata.issuer = v;
		} else if (k == "dcp_metadata_creator") {
			_dcp_metadata.creator = v;
		} else if (k == "dcp_metadata_issue_date") {
			_dcp_metadata.issue_date = v;
		}

		_default_dci_metadata.read_old_metadata (k, v);
	}
}

/** @return Filename to write configuration to */
boost::filesystem::path
Config::file (bool old) const
{
	boost::filesystem::path p;
	p /= g_get_user_config_dir ();
	boost::system::error_code ec;
	boost::filesystem::create_directory (p, ec);
	if (old) {
		p /= ".dvdomatic";
	} else {
		p /= "dcpomatic";
		boost::filesystem::create_directory (p, ec);
		p /= "config.xml";
	}
	return p;
}

boost::filesystem::path
Config::signer_chain_directory () const
{
	boost::filesystem::path p;
	p /= g_get_user_config_dir ();
	p /= "dcpomatic";
	p /= "crypt";
	boost::filesystem::create_directories (p);
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
	LocaleGuard lg;
	
	xmlpp::Document doc;
	xmlpp::Element* root = doc.create_root_node ("Config");

	root->add_child("Version")->add_child_text ("1");
	root->add_child("NumLocalEncodingThreads")->add_child_text (lexical_cast<string> (_num_local_encoding_threads));
	root->add_child("DefaultDirectory")->add_child_text (_default_directory.string ());
	root->add_child("ServerPort")->add_child_text (lexical_cast<string> (_server_port));
	
	for (vector<ServerDescription>::const_iterator i = _servers.begin(); i != _servers.end(); ++i) {
		i->as_xml (root->add_child ("Server"));
	}

	root->add_child("TMSIP")->add_child_text (_tms_ip);
	root->add_child("TMSPath")->add_child_text (_tms_path);
	root->add_child("TMSUser")->add_child_text (_tms_user);
	root->add_child("TMSPassword")->add_child_text (_tms_password);
	if (_sound_processor) {
		root->add_child("SoundProcessor")->add_child_text (_sound_processor->id ());
	}
	if (_language) {
		root->add_child("Language")->add_child_text (_language.get());
	}
	if (_default_container) {
		root->add_child("DefaultContainer")->add_child_text (_default_container->id ());
	}
	if (_default_dcp_content_type) {
		root->add_child("DefaultDCPContentType")->add_child_text (_default_dcp_content_type->dci_name ());
	}
	root->add_child("DCPMetadataIssuer")->add_child_text (_dcp_metadata.issuer);
	root->add_child("DCPMetadataCreator")->add_child_text (_dcp_metadata.creator);

	_default_dci_metadata.as_xml (root->add_child ("DCIMetadata"));

	root->add_child("DefaultStillLength")->add_child_text (lexical_cast<string> (_default_still_length));
	root->add_child("DefaultJ2KBandwidth")->add_child_text (lexical_cast<string> (_default_j2k_bandwidth));

	for (vector<PresetColourConversion>::const_iterator i = _colour_conversions.begin(); i != _colour_conversions.end(); ++i) {
		i->as_xml (root->add_child ("ColourConversion"));
	}

	for (list<shared_ptr<Cinema> >::const_iterator i = _cinemas.begin(); i != _cinemas.end(); ++i) {
		(*i)->as_xml (root->add_child ("Cinema"));
	}

	root->add_child("MailServer")->add_child_text (_mail_server);
	root->add_child("KDMFrom")->add_child_text (_kdm_from);
	root->add_child("KDMEmail")->add_child_text (_kdm_email);

	std::cout << "dcpomatic: writing configuration to " << file(false).string() << "\n";
	doc.write_to_file_formatted (file(false).string ());
	std::cout << "dcpomatic: wrote configuration to " << file(false).string() << "\n";
}

boost::filesystem::path
Config::default_directory_or (boost::filesystem::path a) const
{
	if (_default_directory.empty() || !boost::filesystem::exists (_default_directory)) {
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
