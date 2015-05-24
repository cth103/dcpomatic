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

/** @file src/util.h
 *  @brief Some utility functions and classes.
 */

#ifndef DCPOMATIC_UTIL_H
#define DCPOMATIC_UTIL_H

#include "compose.hpp"
#include "types.h"
#include "exceptions.h"
#include "dcpomatic_time.h"
#include <dcp/util.h>
#include <dcp/picture_mxf_writer.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
}
#include <boost/shared_ptr.hpp>
#include <boost/optional.hpp>
#include <boost/filesystem.hpp>
#include <string>
#include <vector>

#undef check

namespace dcp {
	class PictureMXF;
	class SoundMXF;
	class SubtitleContent;
}

/** The maximum number of audio channels that we can have in a DCP */
#define MAX_DCP_AUDIO_CHANNELS 12
/** Message broadcast to find possible encoding servers */
#define DCPOMATIC_HELLO "Boys, you gotta learn not to talk to nuns that way"
/** Number of films to keep in history */
#define HISTORY_SIZE 10
#define REPORT_PROBLEM _("Please report this problem by using Help -> Report a problem or via email to carl@dcpomatic.com")

extern std::string program_name;

class Job;
struct AVSubtitle;

extern std::string seconds_to_hms (int);
extern std::string seconds_to_approximate_hms (int);
extern double seconds (struct timeval);
extern void dcpomatic_setup ();
extern void dcpomatic_setup_gettext_i18n (std::string);
extern std::string md5_digest_head_tail (std::vector<boost::filesystem::path>, boost::uintmax_t size);
extern void ensure_ui_thread ();
extern std::string audio_channel_name (int);
extern bool valid_image_file (boost::filesystem::path);
extern bool valid_j2k_file (boost::filesystem::path);
#ifdef DCPOMATIC_WINDOWS
extern boost::filesystem::path mo_path ();
#endif
extern std::string tidy_for_filename (std::string);
extern dcp::Size fit_ratio_within (float ratio, dcp::Size);
extern int dcp_audio_frame_rate (int);
extern int stride_round_up (int, int const *, int);
extern int round_to (float n, int r);
extern void* wrapped_av_malloc (size_t);

class FFmpegSubtitlePeriod
{
public:
	FFmpegSubtitlePeriod (ContentTime f)
		: from (f)
	{}

	FFmpegSubtitlePeriod (ContentTime f, ContentTime t)
		: from (f)
		, to (t)
	{}

	ContentTime from;
	boost::optional<ContentTime> to;
};

extern FFmpegSubtitlePeriod subtitle_period (AVSubtitle const &);
extern void set_backtrace_file (boost::filesystem::path);
extern dcp::FrameInfo read_frame_info (FILE* file, int frame, Eyes eyes);
extern void write_frame_info (FILE* file, int frame, Eyes eyes, dcp::FrameInfo info);
extern int64_t video_frames_to_audio_frames (VideoFrame v, float audio_sample_rate, float frames_per_second);
extern std::map<std::string, std::string> split_get_request (std::string url);
extern std::string video_mxf_filename (boost::shared_ptr<dcp::PictureMXF> mxf);
extern std::string audio_mxf_filename (boost::shared_ptr<dcp::SoundMXF> mxf);
extern std::string subtitle_content_filename (boost::shared_ptr<dcp::SubtitleContent> content);

#endif

