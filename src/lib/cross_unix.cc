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


#include "cross.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
extern "C" {
#include <libavformat/avio.h>
}
LIBDCP_ENABLE_WARNINGS
#include <fmt/format.h>


using std::string;


/** @param s Number of seconds to sleep for */
void
dcpomatic_sleep_seconds(int s)
{
	sleep(s);
}


void
dcpomatic_sleep_milliseconds(int ms)
{
	usleep(ms * 1000);
}


uint64_t
thread_id()
{
	return (uint64_t) pthread_self();
}


int
avio_open_boost(AVIOContext** s, boost::filesystem::path file, int flags)
{
	return avio_open(s, file.c_str(), flags);
}


boost::filesystem::path
home_directory()
{
	return getenv("HOME");
}


/** @return true if this process is a 32-bit one running on a 64-bit-capable OS */
bool
running_32_on_64()
{
	/* I'm assuming nobody does this on Linux/macOS */
	return false;
}


string
dcpomatic::get_process_id()
{
	return fmt::to_string(getpid());
}


ArgFixer::ArgFixer(int argc, char** argv)
	: _argc(argc)
	, _argv(argv)
{

}

