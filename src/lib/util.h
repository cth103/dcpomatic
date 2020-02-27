/*
    Copyright (C) 2012-2020 Carl Hetherington <cth@carlh.net>

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
#include "audio_mapping.h"
#include <dcp/util.h>
#include <dcp/subtitle_image.h>
#include <boost/shared_ptr.hpp>
#include <boost/optional.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <string>
#include <map>
#include <vector>

#undef check

namespace dcp {
	class PictureAsset;
	class SoundAsset;
}

#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif

/** The maximum number of audio channels that we can have in a DCP */
#define MAX_DCP_AUDIO_CHANNELS 16
/** Message broadcast to find possible encoding servers */
#define DCPOMATIC_HELLO "I mean really, Ray, it's used."
/** Number of films to keep in history */
#define HISTORY_SIZE 10
#define REPORT_PROBLEM _("Please report this problem by using Help -> Report a problem or via email to carl@dcpomatic.com")
#define TEXT_FONT_ID "font"
/** Largest KDM size (in bytes) that will be accepted */
#define MAX_KDM_SIZE (256 * 1024)
/** Number of lines that closed caption viewers will display */
#define CLOSED_CAPTION_LINES 3
/** Maximum line length of closed caption viewers */
#define CLOSED_CAPTION_LENGTH 30
/* We are mis-using genre here, as only some metadata tags are written/read.
   I tried the use_metadata_tags option but it didn't seem to make any difference.
*/
#define SWAROOP_ID_TAG "genre"

extern std::string program_name;
extern bool is_batch_converter;

struct AVSubtitle;
class AudioBuffers;
class TextDecoder;

extern std::string seconds_to_hms (int);
extern std::string time_to_hmsf (dcpomatic::DCPTime time, Frame rate);
extern std::string seconds_to_approximate_hms (int);
extern double seconds (struct timeval);
extern void dcpomatic_setup ();
extern void dcpomatic_setup_path_encoding ();
extern void dcpomatic_setup_gettext_i18n (std::string);
extern std::string digest_head_tail (std::vector<boost::filesystem::path>, boost::uintmax_t size);
extern void ensure_ui_thread ();
extern std::string audio_channel_name (int);
extern std::string short_audio_channel_name (int);
extern bool valid_image_file (boost::filesystem::path);
extern bool valid_sound_file (boost::filesystem::path);
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
extern std::string careful_string_filter (std::string);
extern std::pair<int, int> audio_channel_types (std::list<int> mapped, int channels);
extern boost::shared_ptr<AudioBuffers> remap (boost::shared_ptr<const AudioBuffers> input, int output_channels, AudioMapping map);
extern Eyes increment_eyes (Eyes e);
extern void checked_fread (void* ptr, size_t size, FILE* stream, boost::filesystem::path path);
extern void checked_fwrite (void const * ptr, size_t size, FILE* stream, boost::filesystem::path path);
extern size_t utf8_strlen (std::string s);
extern std::string day_of_week_to_string (boost::gregorian::greg_weekday d);
extern void emit_subtitle_image (dcpomatic::ContentTimePeriod period, dcp::SubtitleImage sub, dcp::Size size, boost::shared_ptr<TextDecoder> decoder);
extern bool show_jobs_on_console (bool progress);
extern void copy_in_bits (boost::filesystem::path from, boost::filesystem::path to, boost::function<void (float)>);
#ifdef DCPOMATIC_VARIANT_SWAROOP
extern boost::shared_ptr<dcp::CertificateChain> read_swaroop_chain (boost::filesystem::path path);
extern void write_swaroop_chain (boost::shared_ptr<const dcp::CertificateChain> chain, boost::filesystem::path output);
#endif

template <class T>
std::list<T>
vector_to_list (std::vector<T> v)
{
	std::list<T> l;
	BOOST_FOREACH (T& i, v) {
		l.push_back (i);
	}
	return l;
}

extern double db_to_linear (double db);
extern double linear_to_db (double linear);

#endif
