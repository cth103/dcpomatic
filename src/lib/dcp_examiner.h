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
	explicit DCPExaminer (std::shared_ptr<const DCPContent>, bool tolerant);

	bool has_video () const override {
		return _has_video;
	}

	boost::optional<double> video_frame_rate () const override {
		return _video_frame_rate;
	}

	dcp::Size video_size () const override {
		DCPOMATIC_ASSERT (_has_video);
		DCPOMATIC_ASSERT (_video_size);
		return *_video_size;
	}

	Frame video_length () const override {
		return _video_length;
	}

	bool yuv () const override {
		return false;
	}

	VideoRange range () const override {
		return VideoRange::FULL;
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

	Frame audio_length () const override {
		return _audio_length;
	}

	int audio_frame_rate () const override {
		return _audio_frame_rate.get_value_or (48000);
	}

	boost::optional<dcp::LanguageTag> audio_language () const {
		return _audio_language;
	}

	/** @param type TEXT_OPEN_SUBTITLE or TEXT_CLOSED_CAPTION.
	 *  @return Number of assets of this type in this DCP.
	 */
	int text_count (TextType type) const {
		return _text_count[static_cast<int>(type)];
	}

	boost::optional<dcp::LanguageTag> open_subtitle_language () const {
		return _open_subtitle_language;
	}

	DCPTextTrack dcp_text_track (int i) const {
		DCPOMATIC_ASSERT (i >= 0 && i < static_cast<int>(_dcp_text_tracks.size()));
		return _dcp_text_tracks[i];
	}

	bool kdm_valid () const {
		return _kdm_valid;
	}

	boost::optional<dcp::Standard> standard () const {
		return _standard;
	}

	bool three_d () const {
		return _three_d;
	}

	dcp::ContentKind content_kind () const {
		return _content_kind;
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

	/** @return fonts in each reel */
	std::vector<std::vector<std::shared_ptr<dcpomatic::Font>>> fonts() const {
		return _fonts;
	}

private:
	boost::optional<double> _video_frame_rate;
	boost::optional<dcp::Size> _video_size;
	Frame _video_length = 0;
	boost::optional<int> _audio_channels;
	boost::optional<int> _audio_frame_rate;
	Frame _audio_length = 0;
	std::string _name;
	/** true if this DCP has video content (but false if it has unresolved references to video content) */
	bool _has_video = false;
	/** true if this DCP has audio content (but false if it has unresolved references to audio content) */
	bool _has_audio = false;
	boost::optional<dcp::LanguageTag> _audio_language;
	/** number of different assets of each type (OCAP/CCAP) */
	int _text_count[static_cast<int>(TextType::COUNT)];
	boost::optional<dcp::LanguageTag> _open_subtitle_language;
	/** the DCPTextTracks for each of our CCAPs */
	std::vector<DCPTextTrack> _dcp_text_tracks;
	bool _encrypted = false;
	bool _needs_assets = false;
	bool _kdm_valid = false;
	boost::optional<dcp::Standard> _standard;
	bool _three_d = false;
	dcp::ContentKind _content_kind;
	std::string _cpl;
	std::list<int64_t> _reel_lengths;
	std::map<dcp::Marker, dcp::Time> _markers;
	std::vector<dcp::Rating> _ratings;
	std::vector<std::string> _content_versions;
	bool _has_atmos = false;
	Frame _atmos_length = 0;
	dcp::Fraction _atmos_edit_rate;
	std::vector<std::vector<std::shared_ptr<dcpomatic::Font>>> _fonts;
};
