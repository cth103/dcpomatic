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

/** @file src/util.h
 *  @brief Some utility functions and classes.
 */

#ifndef DCPOMATIC_UTIL_H
#define DCPOMATIC_UTIL_H

#include "types.h"
#include "dcpomatic_time.h"
#include <dcp/util.h>
#include <boost/shared_ptr.hpp>
#include <boost/optional.hpp>
#include <boost/filesystem.hpp>
#include <string>
#include <vector>

#undef check

namespace dcp {
	class PictureAsset;
	class SoundAsset;
}

/** The maximum number of audio channels that we can have in a DCP */
#define MAX_DCP_AUDIO_CHANNELS 16
/** Message broadcast to find possible encoding servers */
#define DCPOMATIC_HELLO "I mean really, Ray, it's used."
/** Number of films to keep in history */
#define HISTORY_SIZE 10
#define REPORT_PROBLEM _("Please report this problem by using Help -> Report a problem or via email to carl@dcpomatic.com")
#define TEXT_FONT_ID "font"

extern std::string program_name;

struct AVSubtitle;

extern std::string seconds_to_hms (int);
extern std::string seconds_to_approximate_hms (int);
extern double seconds (struct timeval);
extern void dcpomatic_setup ();
extern void dcpomatic_setup_path_encoding ();
extern void dcpomatic_setup_gettext_i18n (std::string);
extern std::string digest_head_tail (std::vector<boost::filesystem::path>, boost::uintmax_t size);
extern void ensure_ui_thread ();
extern std::string audio_channel_name (int);
extern bool valid_image_file (boost::filesystem::path);
extern bool valid_j2k_file (boost::filesystem::path);
#ifdef DCPOMATIC_WINDOWS
extern boost::filesystem::path mo_path ();
#endif
extern std::string tidy_for_filename (std::string);
extern dcp::Size fit_ratio_within (float ratio, dcp::Size);
extern int stride_round_up (int, int const *, int);
extern void* wrapped_av_malloc (size_t);
extern void set_backtrace_file (boost::filesystem::path);
extern std::map<std::string, std::string> split_get_request (std::string url);
extern std::string video_asset_filename (boost::shared_ptr<dcp::PictureAsset> asset, int reel_index, int reel_count, boost::optional<std::string> content_summary);
extern std::string audio_asset_filename (boost::shared_ptr<dcp::SoundAsset> asset, int reel_index, int reel_count, boost::optional<std::string> content_summary);
extern float relaxed_string_to_float (std::string);
extern bool string_not_empty (std::string);

#endif
