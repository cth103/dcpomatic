/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include "log.h"
#include "compose.hpp"
#include "version.h"
#include "cross.h"
#include <dcp/version.h>
#include <openjpeg.h>
#include <libssh/libssh.h>
#ifdef DCPOMATIC_IMAGE_MAGICK
#include <magick/MagickCore.h>
#else
#include <magick/common.h>
#include <magick/magick_config.h>
#endif
#include <magick/version.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavfilter/avfiltergraph.h>
#include <libavutil/pixfmt.h>
}
#include <boost/thread.hpp>

#include "i18n.h"

#define LOG_GENERAL(...) log->log (String::compose (__VA_ARGS__), Log::TYPE_GENERAL);
#define LOG_GENERAL_NC(...) log->log (__VA_ARGS__, Log::TYPE_GENERAL);

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
	SafeStringStream s;
	s << ((v & 0xff0000) >> 16) << N_(".") << ((v & 0xff00) >> 8) << N_(".") << (v & 0xff);
	return s.str ();
}


/** Return a user-readable string summarising the versions of our dependencies */
static
string
dependency_version_summary ()
{
	SafeStringStream s;
	s << N_("libopenjpeg ") << opj_version () << N_(", ")
	  << N_("libavcodec ") << ffmpeg_version_to_string (avcodec_version()) << N_(", ")
	  << N_("libavfilter ") << ffmpeg_version_to_string (avfilter_version()) << N_(", ")
	  << N_("libavformat ") << ffmpeg_version_to_string (avformat_version()) << N_(", ")
	  << N_("libavutil ") << ffmpeg_version_to_string (avutil_version()) << N_(", ")
	  << N_("libswscale ") << ffmpeg_version_to_string (swscale_version()) << N_(", ")
	  << MagickVersion << N_(", ")
	  << N_("libssh ") << ssh_version (0) << N_(", ")
	  << N_("libdcp ") << dcp::version << N_(" git ") << dcp::git_commit;

	return s.str ();
}

void
environment_info (shared_ptr<Log> log)
{
	LOG_GENERAL ("DCP-o-matic %1 git %2 using %3", dcpomatic_version, dcpomatic_git_commit, dependency_version_summary());

	{
		char buffer[128];
		gethostname (buffer, sizeof (buffer));
		LOG_GENERAL ("Host name %1", buffer);
	}

#ifdef DCPOMATIC_DEBUG
	LOG_GENERAL_NC ("DCP-o-matic built in debug mode.");
#else
	LOG_GENERAL_NC ("DCP-o-matic built in optimised mode.");
#endif
#ifdef LIBDCP_DEBUG
	LOG_GENERAL_NC ("libdcp built in debug mode.");
#else
	LOG_GENERAL_NC ("libdcp built in optimised mode.");
#endif

#ifdef DCPOMATIC_WINDOWS
	OSVERSIONINFO info;
	info.dwOSVersionInfoSize = sizeof (info);
	GetVersionEx (&info);
	LOG_GENERAL ("Windows version %1.%2.%3 SP %4", info.dwMajorVersion, info.dwMinorVersion, info.dwBuildNumber, info.szCSDVersion);
#endif	

#if __GNUC__
#if __x86_64__
	LOG_GENERAL_NC ("Built for 64-bit");
#else
	LOG_GENERAL_NC ("Built for 32-bit");
#endif
#endif
	
	LOG_GENERAL ("CPU: %1, %2 processors", cpu_info(), boost::thread::hardware_concurrency ());
	list<pair<string, string> > const m = mount_info ();
	for (list<pair<string, string> >::const_iterator i = m.begin(); i != m.end(); ++i) {
		LOG_GENERAL ("Mount: %1 %2", i->first, i->second);
	}
}
