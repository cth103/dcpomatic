/*
    Copyright (C) 2019-2020 Carl Hetherington <cth@carlh.net>

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

#include "disk_writer_messages.h"
#include "copy_to_drive_job.h"
#include "compose.hpp"
#include "exceptions.h"
#include <dcp/raw_convert.h>
#include <nanomsg/nn.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

#include "i18n.h"

using std::string;
using std::cout;
using std::min;
using boost::shared_ptr;
using dcp::raw_convert;

CopyToDriveJob::CopyToDriveJob (boost::filesystem::path dcp, Drive drive, Nanomsg& nanomsg)
	: Job (shared_ptr<Film>())
	, _dcp (dcp)
	, _drive (drive)
	, _nanomsg (nanomsg)
{

}

string
CopyToDriveJob::name () const
{
	return String::compose (_("Copying %1 to %2"), _dcp.filename().string(), _drive.description());
}

string
CopyToDriveJob::json_name () const
{
	return N_("copy");
}

void
CopyToDriveJob::run ()
{
	if (!_nanomsg.nonblocking_send(String::compose("W\n%1\n%2\n", _dcp.string(), _drive.internal_name()))) {
		throw CopyError ("Could not communicate with writer process", 0);
	}

	bool formatting = false;
	while (true) {
		string s = _nanomsg.blocking_get ();
		if (s == DISK_WRITER_OK) {
			set_state (FINISHED_OK);
			return;
		} else if (s == DISK_WRITER_ERROR) {
			string const m = _nanomsg.blocking_get ();
			string const n = _nanomsg.blocking_get ();
			throw CopyError (m, raw_convert<int>(n));
		} else if (s == DISK_WRITER_FORMATTING) {
			sub ("Formatting drive");
			set_progress_unknown ();
			formatting = true;
		} else if (s == DISK_WRITER_PROGRESS) {
			if (formatting) {
				sub ("Copying DCP");
				formatting = false;
			}
			set_progress (raw_convert<float>(_nanomsg.blocking_get()));
		}
	}
}
