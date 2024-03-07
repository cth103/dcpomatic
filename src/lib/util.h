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

extern std::string program_name;
extern bool is_batch_converter;

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
extern std::string video_asset_filename (std::shared_ptr<dcp::PictureAsset> asset, int reel_index, int reel_count, boost::optional<std::string> content_summary);
extern std::string audio_asset_filename (std::shared_ptr<dcp::SoundAsset> asset, int reel_index, int reel_count, boost::optional<std::string> content_summary);
extern std::string subtitle_asset_filename (std::shared_ptr<dcp::SubtitleAsset> asset, int reel_index, int reel_count, boost::optional<std::string> content_summary, std::string extension);
extern std::string atmos_asset_filename (std::shared_ptr<dcp::AtmosAsset> asset, int reel_index, int reel_count, boost::optional<std::string> content_summary);
extern std::string careful_string_filter (std::string);
extern std::pair<int, int> audio_channel_types (std::list<int> mapped, int channels);
extern std::shared_ptr<AudioBuffers> remap (std::shared_ptr<const AudioBuffers> input, int output_channels, AudioMapping map);
extern size_t utf8_strlen (std::string s);
extern void emit_subtitle_image (dcpomatic::ContentTimePeriod period, dcp::SubtitleImage sub, dcp::Size size, std::shared_ptr<TextDecoder> decoder);
extern void copy_in_bits (boost::filesystem::path from, boost::filesystem::path to, std::function<void (float)>);
extern dcp::Size scale_for_display (dcp::Size s, dcp::Size display_container, dcp::Size film_container, PixelQuanta quanta);
extern dcp::DecryptedKDM decrypt_kdm_with_helpful_error (dcp::EncryptedKDM kdm);
extern boost::filesystem::path default_font_file ();
extern void start_of_thread (std::string name);
extern std::string error_details(boost::system::error_code ec);
extern bool contains_assetmap(boost::filesystem::path dir);
extern std::string word_wrap(std::string input, int columns);
extern void capture_ffmpeg_logs();
extern std::string screen_names_to_string(std::vector<std::string> names);

#endif
