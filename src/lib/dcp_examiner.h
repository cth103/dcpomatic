/*
    Copyright (C) 2014-2020 Carl Hetherington <cth@carlh.net>

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


/** @file  src/lib/dcp_examiner.h
 *  @brief DCPExaminer class.
 */


#include "audio_examiner.h"
#include "dcp_text_track.h"
#include "dcpomatic_assert.h"
#include "video_examiner.h"
#include <dcp/dcp_time.h>
#include <dcp/rating.h>


class DCPContent;


class DCPExaminer : public VideoExaminer, public AudioExaminer
{
public:
	DCPExaminer(std::shared_ptr<const DCPContent>, bool tolerant);

	bool has_video () const override {
		return _has_video;
	}

	boost::optional<double> video_frame_rate () const override {
		return _video_frame_rate;
	}

	boost::optional<dcp::Size> video_size() const override {
		return _video_size;
	}

	Frame video_length () const override {
		return _video_length;
	}

	bool yuv () const override {
		return false;
	}

	VideoRange range () const override {
		return _video_range;
	}

	PixelQuanta pixel_quanta () const override {
		return {};
	}

	std::string name () const {
		return _name;
	}

	bool encrypted () const {
		return _encrypted;
	}

	bool needs_assets () const {
		return _needs_assets;
	}

	bool has_audio () const override {
		return _has_audio;
	}

	int audio_channels () const override {
		return _audio_channels.get_value_or (0);
	}

	int active_audio_channels() const {
		return _active_audio_channels.get_value_or(0);
	}

	Frame audio_length () const override {
		return _audio_length;
	}

	int audio_frame_rate () const override {
		return _audio_frame_rate.get_value_or (48000);
	}

	boost::optional<dcp::LanguageTag> audio_language () const {
		return _audio_language;
	}

	/*  @return the number of "streams" of @type in the DCP.
	 *  Reels do not affect the return value of this method: if a DCP
	 *  has any subtitles, type=TEXT_OPEN_SUBTITLE will return 1.
	 */
	int text_count (TextType type) const {
		return _text_count[type];
	}

	boost::optional<dcp::LanguageTag> open_subtitle_language () const {
		return _open_subtitle_language;
	}

	boost::optional<dcp::LanguageTag> open_caption_language() const {
		return _open_caption_language;
	}

	DCPTextTrack dcp_subtitle_track(int i) const {
		DCPOMATIC_ASSERT (i >= 0 && i < static_cast<int>(_dcp_subtitle_tracks.size()));
		return _dcp_subtitle_tracks[i];
	}

	DCPTextTrack dcp_caption_track(int i) const {
		DCPOMATIC_ASSERT (i >= 0 && i < static_cast<int>(_dcp_caption_tracks.size()));
		return _dcp_caption_tracks[i];
	}

	bool kdm_valid () const {
		return _kdm_valid;
	}

	boost::optional<dcp::Standard> standard () const {
		return _standard;
	}

	VideoEncoding video_encoding() const {
		return _video_encoding;
	}

	bool three_d () const {
		return _three_d;
	}

	dcp::ContentKind content_kind () const {
		DCPOMATIC_ASSERT(_content_kind);
		return *_content_kind;
	}

	std::string cpl () const {
		return _cpl;
	}

	std::list<int64_t> reel_lengths () const {
		return _reel_lengths;
	}

	std::map<dcp::Marker, dcp::Time> markers () const {
		return _markers;
	}

	std::vector<dcp::Rating> ratings () const {
		return _ratings;
	}

	std::vector<std::string> content_versions () const {
		return _content_versions;
	}

	bool has_atmos () const {
		return _has_atmos;
	}

	Frame atmos_length () const {
		return _atmos_length;
	}

	dcp::Fraction atmos_edit_rate () const {
		return _atmos_edit_rate;
	}

	EnumIndexedVector<bool, TextType> has_non_zero_entry_point() const {
		return _has_non_zero_entry_point;
	}

	void add_fonts(std::shared_ptr<TextContent> content);

private:
	boost::optional<double> _video_frame_rate;
	boost::optional<dcp::Size> _video_size;
	Frame _video_length = 0;
	boost::optional<int> _audio_channels;
	boost::optional<int> _active_audio_channels;
	boost::optional<int> _audio_frame_rate;
	Frame _audio_length = 0;
	std::string _name;
	/** true if this DCP has video content (but false if it has unresolved references to video content) */
	bool _has_video = false;
	/** true if this DCP has audio content (but false if it has unresolved references to audio content) */
	bool _has_audio = false;
	boost::optional<dcp::LanguageTag> _audio_language;
	/** number of different assets of each type (OCAP/CCAP) */
	EnumIndexedVector<int, TextType> _text_count;
	boost::optional<dcp::LanguageTag> _open_subtitle_language;
	boost::optional<dcp::LanguageTag> _open_caption_language;
	/** the DCPTextTracks for each of our closed subtitles */
	std::vector<DCPTextTrack> _dcp_subtitle_tracks;
	/** the DCPTextTracks for each of our closed captions */
	std::vector<DCPTextTrack> _dcp_caption_tracks;
	bool _encrypted = false;
	bool _needs_assets = false;
	bool _kdm_valid = false;
	boost::optional<dcp::Standard> _standard;
	VideoEncoding _video_encoding = VideoEncoding::JPEG2000;
	bool _three_d = false;
	boost::optional<dcp::ContentKind> _content_kind;
	std::string _cpl;
	std::list<int64_t> _reel_lengths;
	std::map<dcp::Marker, dcp::Time> _markers;
	std::vector<dcp::Rating> _ratings;
	std::vector<std::string> _content_versions;
	bool _has_atmos = false;
	Frame _atmos_length = 0;
	dcp::Fraction _atmos_edit_rate;
	EnumIndexedVector<bool, TextType> _has_non_zero_entry_point;
	VideoRange _video_range = VideoRange::FULL;

	struct Font
	{
		Font(int reel_index_, std::string asset_id_, std::shared_ptr<dcpomatic::Font> font_)
			: reel_index(reel_index_)
			, asset_id(asset_id_)
			, font(font_)
		{}

		int reel_index;
		std::string asset_id;
		std::shared_ptr<dcpomatic::Font> font;
	};

	std::vector<Font> _fonts;
};
