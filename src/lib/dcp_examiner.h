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

#include "video_examiner.h"
#include "audio_examiner.h"
#include "dcp.h"
#include "dcp_text_track.h"
#include "dcpomatic_assert.h"
#include <dcp/dcp_time.h>

class DCPContent;

class DCPExaminer : public DCP, public VideoExaminer, public AudioExaminer
{
public:
	explicit DCPExaminer (boost::shared_ptr<const DCPContent>, bool tolerant);

	bool has_video () const {
		return _has_video;
	}

	boost::optional<double> video_frame_rate () const {
		return _video_frame_rate;
	}

	dcp::Size video_size () const {
		DCPOMATIC_ASSERT (_has_video);
		DCPOMATIC_ASSERT (_video_size);
		return *_video_size;
	}

	Frame video_length () const {
		return _video_length;
	}

	bool yuv () const {
		return false;
	}

	VideoRange range () const {
		return VIDEO_RANGE_FULL;
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

	bool has_audio () const {
		return _has_audio;
	}

	int audio_channels () const {
		return _audio_channels.get_value_or (0);
	}

	Frame audio_length () const {
		return _audio_length;
	}

	int audio_frame_rate () const {
		return _audio_frame_rate.get_value_or (48000);
	}

	/** @param type TEXT_OPEN_SUBTITLE or TEXT_CLOSED_CAPTION.
	 *  @return Number of assets of this type in this DCP.
	 */
	int text_count (TextType type) const {
		return _text_count[type];
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

	std::string content_version () const {
		return _content_version;
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

private:
	boost::optional<double> _video_frame_rate;
	boost::optional<dcp::Size> _video_size;
	Frame _video_length;
	boost::optional<int> _audio_channels;
	boost::optional<int> _audio_frame_rate;
	Frame _audio_length;
	std::string _name;
	/** true if this DCP has video content (but false if it has unresolved references to video content) */
	bool _has_video;
	/** true if this DCP has audio content (but false if it has unresolved references to audio content) */
	bool _has_audio;
	/** number of different assets of each type (OCAP/CCAP) */
	int _text_count[TEXT_COUNT];
	/** the DCPTextTracks for each of our CCAPs */
	std::vector<DCPTextTrack> _dcp_text_tracks;
	bool _encrypted;
	bool _needs_assets;
	bool _kdm_valid;
	boost::optional<dcp::Standard> _standard;
	bool _three_d;
	dcp::ContentKind _content_kind;
	std::string _cpl;
	std::list<int64_t> _reel_lengths;
	std::map<dcp::Marker, dcp::Time> _markers;
	std::vector<dcp::Rating> _ratings;
	std::string _content_version;
	bool _has_atmos;
	Frame _atmos_length;
	dcp::Fraction _atmos_edit_rate;
};
