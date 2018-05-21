/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include "log.h"
#include "compose.hpp"
#include "version.h"
#include "cross.h"
#include <dcp/version.h>
#include <libssh/libssh.h>
#ifdef DCPOMATIC_IMAGE_MAGICK
/* ImageMagick */
#ifdef DCPOMATIC_MAGICKCORE_MAGICK
#include <magick/MagickCore.h>
#include <magick/version.h>
#else
#include <MagickCore/MagickCore.h>
#include <MagickCore/version.h>
#endif
#else
/* GraphicsMagick */
#include <magick/common.h>
#include <magick/magick_config.h>
#include <magick/version.h>
#endif
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavfilter/avfilter.h>
#include <libavutil/pixfmt.h>
}
#include <boost/thread.hpp>

#include "i18n.h"

#define LOG_GENERAL(...) log->log (String::compose (__VA_ARGS__), LogEntry::TYPE_GENERAL);
#define LOG_GENERAL_NC(...) log->log (__VA_ARGS__, LogEntry::TYPE_GENERAL);

using std::string;
using std::list;
using std::pair;
using boost::shared_ptr;

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
		buffer, sizeof(buffer), "libavcodec %s, libavfilter %s, libavformat %s, libavutil %s, libswscale %s, %s, libssh %s, libdcp %s git %s",
		ffmpeg_version_to_string(avcodec_version()).c_str(),
		ffmpeg_version_to_string(avfilter_version()).c_str(),
		ffmpeg_version_to_string(avformat_version()).c_str(),
		ffmpeg_version_to_string(avutil_version()).c_str(),
		ffmpeg_version_to_string(swscale_version()).c_str(),
		MagickVersion,
		ssh_version(0),
		dcp::version, dcp::git_commit
		);

	return buffer;
}

list<string>
environment_info ()
{
	list<string> info;

	info.push_back (String::compose ("DCP-o-matic %1 git %2 using %3", dcpomatic_version, dcpomatic_git_commit, dependency_version_summary()));

	{
		char buffer[128];
		gethostname (buffer, sizeof (buffer));
		info.push_back (String::compose ("Host name %1", &buffer[0]));
	}

#ifdef DCPOMATIC_DEBUG
	info.push_back ("DCP-o-matic built in debug mode.");
#else
	info.push_back ("DCP-o-matic built in optimised mode.");
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
	info.push_back ("Built for 64-bit");
#else
	info.push_back ("Built for 32-bit");
#endif
#endif

	info.push_back (String::compose ("CPU: %1, %2 processors", cpu_info(), boost::thread::hardware_concurrency ()));
	list<pair<string, string> > const m = mount_info ();
	for (list<pair<string, string> >::const_iterator i = m.begin(); i != m.end(); ++i) {
		info.push_back (String::compose ("Mount: %1 %2", i->first, i->second));
	}

	return info;
}
