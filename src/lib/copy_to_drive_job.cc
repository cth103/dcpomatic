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


#include "compose.hpp"
#include "copy_to_drive_job.h"
#include "dcpomatic_log.h"
#include "disk_writer_messages.h"
#include "exceptions.h"
#include <dcp/raw_convert.h>
#include <nanomsg/nn.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i18n.h"


using std::cout;
using std::min;
using std::shared_ptr;
using std::string;
using boost::optional;
using dcp::raw_convert;


CopyToDriveJob::CopyToDriveJob(std::vector<boost::filesystem::path> const& dcps, Drive drive, Nanomsg& nanomsg)
	: Job (shared_ptr<Film>())
	, _dcps (dcps)
	, _drive (drive)
	, _nanomsg (nanomsg)
{

}


string
CopyToDriveJob::name () const
{
	if (_dcps.size() == 1) {
		return String::compose(_("Copying %1\nto %2"), _dcps[0].filename().string(), _drive.description());
	}

	return String::compose(_("Copying DCPs to %1"), _drive.description());
}


string
CopyToDriveJob::json_name () const
{
	return N_("copy");
}

void
CopyToDriveJob::run ()
{
	LOG_DISK("Sending write requests to disk %1 for:", _drive.device());
	for (auto dcp: _dcps) {
		LOG_DISK("%1", dcp.string());
	}

	string request = String::compose(DISK_WRITER_WRITE "\n%1\n", _drive.device());
	for (auto dcp: _dcps) {
		request += String::compose("%1\n", dcp.string());
	}
	request += "\n";
	if (!_nanomsg.send(request, 2000)) {
		LOG_DISK_NC("Failed to send write request.");
		throw CommunicationFailedError ();
	}

	enum State {
		SETUP,
		FORMAT,
		COPY,
		VERIFY
	} state = SETUP;

	while (true) {
		optional<string> s = _nanomsg.receive (10000);
		if (!s) {
			continue;
		}
		if (*s == DISK_WRITER_OK) {
			set_state (FINISHED_OK);
			return;
		} else if (*s == DISK_WRITER_ERROR) {
			auto const m = _nanomsg.receive (500);
			auto const n = _nanomsg.receive (500);
			throw CopyError (m.get_value_or("Unknown"), raw_convert<int>(n.get_value_or("0")));
		} else if (*s == DISK_WRITER_FORMAT_PROGRESS) {
			if (state == SETUP) {
				sub (_("Formatting drive"));
				state = FORMAT;
			}
			auto progress = _nanomsg.receive (500);
			if (progress) {
				set_progress (raw_convert<float>(*progress));
			}
		} else if (*s == DISK_WRITER_COPY_PROGRESS) {
			if (state == FORMAT) {
				sub (_("Copying DCP"));
				state = COPY;
			}
			auto progress = _nanomsg.receive (500);
			if (progress) {
				set_progress (raw_convert<float>(*progress));
			}
		} else if (*s == DISK_WRITER_VERIFY_PROGRESS) {
			if (state == COPY) {
				sub (_("Verifying copied files"));
				state = VERIFY;
			}
			auto progress = _nanomsg.receive (500);
			if (progress) {
				set_progress (raw_convert<float>(*progress));
			}
		}
	}
}
