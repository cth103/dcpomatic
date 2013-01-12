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
#include "sound_processor.h"

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
	, _reference_scaler (Scaler::from_id ("bicubic"))
	, _tms_path (".")
	, _sound_processor (SoundProcessor::from_id ("dolby_cp750"))
{
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

		if (k == "num_local_encoding_threads") {
			_num_local_encoding_threads = atoi (v.c_str ());
		} else if (k == "default_directory") {
			_default_directory = v;
		} else if (k == "server_port") {
			_server_port = atoi (v.c_str ());
		} else if (k == "reference_scaler") {
			_reference_scaler = Scaler::from_id (v);
		} else if (k == "reference_filter") {
			_reference_filters.push_back (Filter::from_id (v));
		} else if (k == "server") {
			_servers.push_back (ServerDescription::create_from_metadata (v));
		} else if (k == "tms_ip") {
			_tms_ip = v;
		} else if (k == "tms_path") {
			_tms_path = v;
		} else if (k == "tms_user") {
			_tms_user = v;
		} else if (k == "tms_password") {
			_tms_password = v;
		} else if (k == "sound_processor") {
			_sound_processor = SoundProcessor::from_id (v);
		}
	}
}

/** @return Filename to write configuration to */
string
Config::file () const
{
	boost::filesystem::path p;
	p /= g_get_user_config_dir ();
	p /= ".dvdomatic";
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
	f << "num_local_encoding_threads " << _num_local_encoding_threads << "\n"
	  << "default_directory " << _default_directory << "\n"
	  << "server_port " << _server_port << "\n"
	  << "reference_scaler " << _reference_scaler->id () << "\n";

	for (vector<Filter const *>::const_iterator i = _reference_filters.begin(); i != _reference_filters.end(); ++i) {
		f << "reference_filter " << (*i)->id () << "\n";
	}
	
	for (vector<ServerDescription*>::const_iterator i = _servers.begin(); i != _servers.end(); ++i) {
		f << "server " << (*i)->as_metadata () << "\n";
	}

	f << "tms_ip " << _tms_ip << "\n";
	f << "tms_path " << _tms_path << "\n";
	f << "tms_user " << _tms_user << "\n";
	f << "tms_password " << _tms_password << "\n";
	f << "sound_processor " << _sound_processor->id ();
}

string
Config::default_directory_or (string a) const
{
	if (_default_directory.empty() || !boost::filesystem::exists (_default_directory)) {
		return a;
	}

	return _default_directory;
}
