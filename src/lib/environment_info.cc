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


#include "compose.hpp"
#include "cross.h"
#include "log.h"
#include "variant.h"
#include "version.h"
#include <dcp/version.h>
#include <dcp/warnings.h>
#include <libssh/libssh.h>
LIBDCP_DISABLE_WARNINGS
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>
}
LIBDCP_ENABLE_WARNINGS
#include <fmt/format.h>
#include <boost/thread.hpp>

#include "i18n.h"


using std::list;
using std::pair;
using std::shared_ptr;
using std::string;


/** @param v Version as used by FFmpeg.
 *  @return A string representation of v.
 */
static
string
ffmpeg_version_to_string (int v)
{
	char buffer[64];
	snprintf (buffer, sizeof(buffer), "%d.%d.%d", ((v & 0xff0000) >> 16), ((v & 0xff00) >> 8), (v & 0xff));
	return buffer;
}


/** Return a user-readable string summarising the versions of our dependencies */
static
string
dependency_version_summary ()
{
	char buffer[512];
	snprintf (
		buffer, sizeof(buffer), "libavcodec %s, libavfilter %s, libavformat %s, libavutil %s, libswscale %s, libssh %s, libdcp %s git %s",
		ffmpeg_version_to_string(avcodec_version()).c_str(),
		ffmpeg_version_to_string(avfilter_version()).c_str(),
		ffmpeg_version_to_string(avformat_version()).c_str(),
		ffmpeg_version_to_string(avutil_version()).c_str(),
		ffmpeg_version_to_string(swscale_version()).c_str(),
		ssh_version(0),
		dcp::version, dcp::git_commit
		);

	return buffer;
}


list<string>
environment_info ()
{
	list<string> info;

	info.push_back(String::compose("%1 %2 git %3 using %4", variant::dcpomatic(), dcpomatic_version, dcpomatic_git_commit, dependency_version_summary()));

	{
		char buffer[128];
		gethostname (buffer, sizeof (buffer));
		info.push_back (String::compose ("Host name %1", &buffer[0]));
	}

#ifdef DCPOMATIC_DEBUG
	info.push_back(variant::insert_dcpomatic("%1 built in debug mode."));
#else
	info.push_back(variant::insert_dcpomatic("%1 built in optimised mode."));
#endif
#ifdef LIBDCP_DEBUG
	info.push_back ("libdcp built in debug mode.");
#else
	info.push_back ("libdcp built in optimised mode.");
#endif

#ifdef DCPOMATIC_WINDOWS
	OSVERSIONINFO os_info;
	os_info.dwOSVersionInfoSize = sizeof (os_info);
	GetVersionEx (&os_info);
	info.push_back (
		String::compose (
			"Windows version %1.%2.%3",
			(int) os_info.dwMajorVersion, (int) os_info.dwMinorVersion, (int) os_info.dwBuildNumber
			)
		);
	if (os_info.dwMajorVersion == 5 && os_info.dwMinorVersion == 0) {
		info.push_back ("Windows 2000");
	} else if (os_info.dwMajorVersion == 5 && os_info.dwMinorVersion == 1) {
		info.push_back ("Windows XP");
	} else if (os_info.dwMajorVersion == 5 && os_info.dwMinorVersion == 2) {
		info.push_back ("Windows XP 64-bit or Windows Server 2003");
	} else if (os_info.dwMajorVersion == 6 && os_info.dwMinorVersion == 0) {
		info.push_back ("Windows Vista or Windows Server 2008");
	} else if (os_info.dwMajorVersion == 6 && os_info.dwMinorVersion == 1) {
		info.push_back ("Windows 7 or Windows Server 2008");
	} else if (os_info.dwMajorVersion == 6 && (os_info.dwMinorVersion == 2 || os_info.dwMinorVersion == 3)) {
		info.push_back ("Windows 8 or Windows Server 2012");
	} else if (os_info.dwMajorVersion == 10 && os_info.dwMinorVersion == 0) {
		info.push_back ("Windows 10 or Windows Server 2016");
	}
#endif

#if __GNUC__
#if __x86_64__
	info.push_back ("Built for x86 64-bit");
#elif __aarch64__
	info.push_back ("Built for ARM 64-bit");
#else
	info.push_back ("Built for x86 32-bit");
#endif
#endif

	info.push_back (String::compose ("CPU: %1, %2 processors", cpu_info(), boost::thread::hardware_concurrency()));
	for (auto const& i: mount_info()) {
		info.push_back (String::compose("Mount: %1 %2", i.first, i.second));
	}

	return info;
}
