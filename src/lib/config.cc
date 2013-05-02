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
#include "config.h"
#include "server.h"
#include "scaler.h"
#include "filter.h"
#include "format.h"
#include "dcp_content_type.h"
#include "sound_processor.h"

#include "i18n.h"

using std::vector;
using std::ifstream;
using std::string;
using std::ofstream;
using boost::shared_ptr;

Config* Config::_instance = 0;

/** Construct default configuration */
Config::Config ()
	: _num_local_encoding_threads (2)
	, _server_port (6192)
	, _reference_scaler (Scaler::from_id (N_("bicubic")))
	, _tms_path (N_("."))
	, _sound_processor (SoundProcessor::from_id (N_("dolby_cp750")))
	, _default_format (0)
	, _default_dcp_content_type (0)
{
	_allowed_dcp_frame_rates.push_back (24);
	_allowed_dcp_frame_rates.push_back (25);
	_allowed_dcp_frame_rates.push_back (30);
	_allowed_dcp_frame_rates.push_back (48);
	_allowed_dcp_frame_rates.push_back (50);
	_allowed_dcp_frame_rates.push_back (60);
	
	ifstream f (file().c_str ());
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
		} else if (k == N_("reference_scaler")) {
			_reference_scaler = Scaler::from_id (v);
		} else if (k == N_("reference_filter")) {
			_reference_filters.push_back (Filter::from_id (v));
		} else if (k == N_("server")) {
			_servers.push_back (ServerDescription::create_from_metadata (v));
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
		} else if (k == "default_format") {
			_default_format = Format::from_metadata (v);
		} else if (k == "default_dcp_content_type") {
			_default_dcp_content_type = DCPContentType::from_dci_name (v);
		}

		_default_dci_metadata.read (k, v);
	}
}

/** @return Filename to write configuration to */
string
Config::file () const
{
	boost::filesystem::path p;
	p /= g_get_user_config_dir ();
	p /= N_(".dvdomatic");
	return p.string ();
}

/** @return Singleton instance */
Config *
Config::instance ()
{
	if (_instance == 0) {
		_instance = new Config;
	}

	return _instance;
}

/** Write our configuration to disk */
void
Config::write () const
{
	ofstream f (file().c_str ());
	f << N_("num_local_encoding_threads ") << _num_local_encoding_threads << N_("\n")
	  << N_("default_directory ") << _default_directory << N_("\n")
	  << N_("server_port ") << _server_port << N_("\n");

	if (_reference_scaler) {
		f << "reference_scaler " << _reference_scaler->id () << "\n";
	}

	for (vector<Filter const *>::const_iterator i = _reference_filters.begin(); i != _reference_filters.end(); ++i) {
		f << N_("reference_filter ") << (*i)->id () << N_("\n");
	}
	
	for (vector<ServerDescription*>::const_iterator i = _servers.begin(); i != _servers.end(); ++i) {
		f << N_("server ") << (*i)->as_metadata () << N_("\n");
	}

	f << N_("tms_ip ") << _tms_ip << N_("\n");
	f << N_("tms_path ") << _tms_path << N_("\n");
	f << N_("tms_user ") << _tms_user << N_("\n");
	f << N_("tms_password ") << _tms_password << N_("\n");
	if (_sound_processor) {
		f << "sound_processor " << _sound_processor->id () << "\n";
	}
	if (_language) {
		f << "language " << _language.get() << "\n";
	}
	if (_default_format) {
		f << "default_format " << _default_format->as_metadata() << "\n";
	}
	if (_default_dcp_content_type) {
		f << "default_dcp_content_type " << _default_dcp_content_type->dci_name() << "\n";
	}

	_default_dci_metadata.write (f);
}

string
Config::default_directory_or (string a) const
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
