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


#include "audio_mapping.h"
#include "dcpomatic_time.h"
#include "pixel_quanta.h"
#include "types.h"
#include <libcxml/cxml.h>
#include <dcp/atmos_asset.h>
#include <dcp/decrypted_kdm.h>
#include <dcp/util.h>
#include <dcp/subtitle_image.h>
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
	class SubtitleAsset;
}

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
#define MAX_CLOSED_CAPTION_LINES 3
/** Maximum line length of closed caption viewers, according to SMPTE Bv2.1 */
#define MAX_CLOSED_CAPTION_LENGTH 32
/** Maximum size of a subtitle / closed caption MXF in bytes, according to SMPTE Bv2.1 */
#define MAX_TEXT_MXF_SIZE (115 * 1024 * 1024)
#define MAX_TEXT_MXF_SIZE_TEXT "115MB"
/** Maximum size of a font file, in bytes */
#define MAX_FONT_FILE_SIZE (640 * 1024)
#define MAX_FONT_FILE_SIZE_TEXT "640KB"
/** Maximum size of the XML part of a closed caption file, according to SMPTE Bv2.1 */
#define MAX_CLOSED_CAPTION_XML_SIZE (256 * 1024)
#define MAX_CLOSED_CAPTION_XML_SIZE_TEXT "256KB"
#define CERTIFICATE_VALIDITY_PERIOD (10 * 365)

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
extern std::string simple_digest (std::vector<boost::filesystem::path> paths);
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
extern void set_backtrace_file (boost::filesystem::path);
extern std::map<std::string, std::string> split_get_request (std::string url);
extern std::string video_asset_filename (std::shared_ptr<dcp::PictureAsset> asset, int reel_index, int reel_count, boost::optional<std::string> content_summary);
extern std::string audio_asset_filename (std::shared_ptr<dcp::SoundAsset> asset, int reel_index, int reel_count, boost::optional<std::string> content_summary);
extern std::string subtitle_asset_filename (std::shared_ptr<dcp::SubtitleAsset> asset, int reel_index, int reel_count, boost::optional<std::string> content_summary, std::string extension);
extern std::string atmos_asset_filename (std::shared_ptr<dcp::AtmosAsset> asset, int reel_index, int reel_count, boost::optional<std::string> content_summary);
extern float relaxed_string_to_float (std::string);
extern std::string careful_string_filter (std::string);
extern std::pair<int, int> audio_channel_types (std::list<int> mapped, int channels);
extern std::shared_ptr<AudioBuffers> remap (std::shared_ptr<const AudioBuffers> input, int output_channels, AudioMapping map);
extern size_t utf8_strlen (std::string s);
extern std::string day_of_week_to_string (boost::gregorian::greg_weekday d);
extern void emit_subtitle_image (dcpomatic::ContentTimePeriod period, dcp::SubtitleImage sub, dcp::Size size, std::shared_ptr<TextDecoder> decoder);
extern bool show_jobs_on_console (bool progress);
extern void copy_in_bits (boost::filesystem::path from, boost::filesystem::path to, std::function<void (float)>);
extern dcp::Size scale_for_display (dcp::Size s, dcp::Size display_container, dcp::Size film_container, PixelQuanta quanta);
extern dcp::DecryptedKDM decrypt_kdm_with_helpful_error (dcp::EncryptedKDM kdm);
extern boost::filesystem::path default_font_file ();
extern std::string to_upper (std::string s);
extern void start_of_thread (std::string name);
extern void capture_asdcp_logs ();
extern std::string error_details(boost::system::error_code ec);

template <class T>
std::list<T>
vector_to_list (std::vector<T> v)
{
	std::list<T> l;
	for (auto& i: v) {
		l.push_back (i);
	}
	return l;
}

template <class T>
std::vector<T>
list_to_vector (std::list<T> v)
{
	std::vector<T> l;
	for (auto& i: v) {
		l.push_back (i);
	}
	return l;
}

template <class T>
T
number_attribute(cxml::ConstNodePtr node, std::string name1, std::string name2)
{
	auto value = node->optional_number_attribute<T>(name1);
	if (!value) {
		value = node->number_attribute<T>(name2);
	}
	return *value;
}

#endif
