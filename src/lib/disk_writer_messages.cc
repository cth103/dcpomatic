/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

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


#include "dcpomatic_assert.h"
#include "disk_writer_messages.h"
#include "nanomsg.h"
#include <dcp/raw_convert.h>
#include <fmt/format.h>


using std::string;
using boost::optional;


boost::optional<DiskWriterBackEndResponse>
DiskWriterBackEndResponse::read_from_nanomsg(Nanomsg& nanomsg, int timeout)
{
	auto s = nanomsg.receive(timeout);
	if (!s) {
		return {};
	}
	if (*s == DISK_WRITER_OK) {
		return DiskWriterBackEndResponse::ok();
	} else if (*s == DISK_WRITER_ERROR) {
		auto const m = nanomsg.receive(500);
		auto const n = nanomsg.receive(500);
		auto const p = nanomsg.receive(500);
		return DiskWriterBackEndResponse::error(m.get_value_or(""), dcp::raw_convert<int>(n.get_value_or("0")), dcp::raw_convert<int>(p.get_value_or("0")));
	} else if (*s == DISK_WRITER_PONG) {
		return DiskWriterBackEndResponse::pong();
	} else if (*s == DISK_WRITER_FORMAT_PROGRESS) {
		auto progress = nanomsg.receive(500);
		return DiskWriterBackEndResponse::format_progress(dcp::raw_convert<float>(progress.get_value_or("0")));
	} else if (*s == DISK_WRITER_COPY_PROGRESS) {
		auto progress = nanomsg.receive(500);
		return DiskWriterBackEndResponse::copy_progress(dcp::raw_convert<float>(progress.get_value_or("0")));
	} else if (*s == DISK_WRITER_VERIFY_PROGRESS) {
		auto progress = nanomsg.receive(500);
		return DiskWriterBackEndResponse::verify_progress(dcp::raw_convert<float>(progress.get_value_or("0")));
	} else {
		DCPOMATIC_ASSERT(false);
	}

	return {};
}


/** @return true if the message was sent, false if there was a timeout */
bool
DiskWriterBackEndResponse::write_to_nanomsg(Nanomsg& nanomsg, int timeout) const
{
	string message;

	switch (_type)
	{
		case Type::OK:
			message = fmt::format("{}\n", DISK_WRITER_OK);
			break;
		case Type::ERROR:
			message = fmt::format("{}\n{}\n{}\n{}\n", DISK_WRITER_ERROR, _error_message, _ext4_error_number, _platform_error_number);
			break;
		case Type::PONG:
			message = fmt::format("{}\n", DISK_WRITER_PONG);
			break;
		case Type::FORMAT_PROGRESS:
			message = fmt::format("{}\n", DISK_WRITER_FORMAT_PROGRESS);
			message += fmt::to_string(_progress) + "\n";
			break;
		case Type::COPY_PROGRESS:
			message = fmt::format("{}\n", DISK_WRITER_COPY_PROGRESS);
			message += fmt::to_string(_progress) + "\n";
			break;
		case Type::VERIFY_PROGRESS:
			message = fmt::format("{}\n", DISK_WRITER_VERIFY_PROGRESS);
			message += fmt::to_string(_progress) + "\n";
			break;
	}


	return nanomsg.send(message, timeout);
}


