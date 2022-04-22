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


#include "scp_uploader.h"
#include "exceptions.h"
#include "job.h"
#include "config.h"
#include "cross.h"
#include "compose.hpp"
#include <sys/stat.h>

#include "i18n.h"


using std::function;
using std::min;
using std::shared_ptr;
using std::string;


SCPUploader::SCPUploader (function<void (string)> set_status, function<void (float)> set_progress)
	: Uploader (set_status, set_progress)
{
	_session = ssh_new ();
	if (!_session) {
		throw NetworkError (String::compose(_("SSH error [%1]"), "ssh_new"));
	}

	ssh_options_set (_session, SSH_OPTIONS_HOST, Config::instance()->tms_ip().c_str());
	ssh_options_set (_session, SSH_OPTIONS_USER, Config::instance()->tms_user().c_str());
	int const port = 22;
	ssh_options_set (_session, SSH_OPTIONS_PORT, &port);

	int r = ssh_connect (_session);
	if (r != SSH_OK) {
		throw NetworkError (String::compose(_("Could not connect to server %1 (%2)"), Config::instance()->tms_ip(), ssh_get_error(_session)));
	}

LIBDCP_DISABLE_WARNINGS
	r = ssh_is_server_known (_session);
	if (r == SSH_SERVER_ERROR) {
		throw NetworkError (String::compose(_("SSH error [%1] (%2)"), "ssh_is_server_known", ssh_get_error(_session)));
	}
LIBDCP_ENABLE_WARNINGS

	r = ssh_userauth_password (_session, 0, Config::instance()->tms_password().c_str ());
	if (r != SSH_AUTH_SUCCESS) {
		throw NetworkError (String::compose(_("Failed to authenticate with server (%1)"), ssh_get_error(_session)));
	}

	_scp = ssh_scp_new (_session, SSH_SCP_WRITE | SSH_SCP_RECURSIVE, Config::instance()->tms_path().c_str());
	if (!_scp) {
		throw NetworkError (String::compose(_("SSH error [%1] (%2)"), "ssh_scp_new", ssh_get_error(_session)));
	}

	r = ssh_scp_init (_scp);
	if (r != SSH_OK) {
		throw NetworkError (String::compose(_("SSH error [%1] (%2)"), "ssh_scp_init", ssh_get_error(_session)));
	}
}


SCPUploader::~SCPUploader ()
{
	ssh_scp_free (_scp);
	ssh_disconnect (_session);
	ssh_free (_session);
}


void
SCPUploader::create_directory (boost::filesystem::path directory)
{
	/* Use generic_string so that we get forward-slashes in the path, even on Windows */
	int const r = ssh_scp_push_directory (_scp, directory.generic_string().c_str(), S_IRWXU);
	if (r != SSH_OK) {
		throw NetworkError (String::compose(_("Could not create remote directory %1 (%2)"), directory, ssh_get_error(_session)));
	}
}


void
SCPUploader::upload_file (boost::filesystem::path from, boost::filesystem::path to, boost::uintmax_t& transferred, boost::uintmax_t total_size)
{
	auto to_do = boost::filesystem::file_size (from);
	/* Use generic_string so that we get forward-slashes in the path, even on Windows */
	ssh_scp_push_file (_scp, to.generic_string().c_str(), to_do, S_IRUSR | S_IWUSR);

	auto f = fopen_boost (from, "rb");
	if (f == nullptr) {
		throw NetworkError (String::compose(_("Could not open %1 to send"), from));
	}

	boost::uintmax_t buffer_size = 64 * 1024;
	char buffer[buffer_size];

	while (to_do > 0) {
		int const t = min (to_do, buffer_size);
		size_t const read = fread (buffer, 1, t, f);
		if (read != size_t (t)) {
			fclose (f);
			throw ReadFileError (from);
		}

		int const r = ssh_scp_write (_scp, buffer, t);
		if (r != SSH_OK) {
			fclose (f);
			throw NetworkError (String::compose(_("Could not write to remote file (%1)"), ssh_get_error(_session)));
		}
		to_do -= t;
		transferred += t;

		if (total_size > 0) {
			_set_progress ((double) transferred / total_size);
		}
	}

	fclose (f);
}
