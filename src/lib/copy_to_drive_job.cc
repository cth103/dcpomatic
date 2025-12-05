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


#include "copy_to_drive_job.h"
#include "dcpomatic_log.h"
#include "disk_writer_messages.h"
#include "exceptions.h"
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
		return fmt::format(_("Copying {}\nto {}"), _dcps[0].filename().string(), _drive.description());
	}

	return fmt::format(_("Copying DCPs to {}"), _drive.description());
}


string
CopyToDriveJob::json_name () const
{
	return N_("copy");
}

void
CopyToDriveJob::run ()
{
	LOG_DISK("Sending write requests to disk {} for:", _drive.device());
	for (auto dcp: _dcps) {
		LOG_DISK("{}", dcp.string());
	}

	string request = fmt::format(DISK_WRITER_WRITE "\n{}\n", _drive.device());
	for (auto dcp: _dcps) {
		request += fmt::format("{}\n", dcp.string());
	}
	request += "\n";
	if (!_nanomsg.send(request, 2000)) {
		LOG_DISK("Failed to send write request.");
		throw CommunicationFailedError ();
	}

	enum State {
		SETUP,
		FORMAT,
		COPY,
		VERIFY
	} state = SETUP;

	while (true) {
		auto response = DiskWriterBackEndResponse::read_from_nanomsg(_nanomsg, 10000);
		if (!response) {
			continue;
		}

		switch (response->type()) {
		case DiskWriterBackEndResponse::Type::OK:
			set_state (FINISHED_OK);
			return;
		case DiskWriterBackEndResponse::Type::PONG:
			break;
		case DiskWriterBackEndResponse::Type::ERROR:
			throw CopyError(response->error_message(), response->ext4_error_number(), response->platform_error_number());
		case DiskWriterBackEndResponse::Type::FORMAT_PROGRESS:
			if (state == SETUP) {
				sub (_("Formatting drive"));
				state = FORMAT;
			}
			set_progress(response->progress());
			break;
		case DiskWriterBackEndResponse::Type::COPY_PROGRESS:
			if (state == FORMAT) {
				sub (_("Copying DCP"));
				state = COPY;
			}
			set_progress(response->progress());
			break;
		case DiskWriterBackEndResponse::Type::VERIFY_PROGRESS:
			if (state == COPY) {
				sub (_("Verifying copied files"));
				state = VERIFY;
			}
			set_progress(response->progress());
			break;
		}
	}
}
