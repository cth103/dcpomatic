/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

class DCPContent;

class DCPExaminer : public DCP, public VideoExaminer, public AudioExaminer
{
public:
	DCPExaminer (boost::shared_ptr<const DCPContent>);

	boost::optional<double> video_frame_rate () const {
		return _video_frame_rate;
	}

	dcp::Size video_size () const {
		return _video_size.get_value_or (dcp::Size (1998, 1080));
	}

	Frame video_length () const {
		return _video_length;
	}

	bool yuv () const {
		return false;
	}

	std::string name () const {
		return _name;
	}

	bool has_subtitles () const {
		return _has_subtitles;
	}

	bool encrypted () const {
		return _encrypted;
	}

	bool needs_assets () const {
		return _needs_assets;
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

	bool kdm_valid () const {
		return _kdm_valid;
	}

	boost::optional<dcp::Standard> standard () const {
		return _standard;
	}

	bool three_d () const {
		return _three_d;
	}

	std::string cpl () const {
		return _cpl;
	}

private:
	boost::optional<double> _video_frame_rate;
	boost::optional<dcp::Size> _video_size;
	Frame _video_length;
	boost::optional<int> _audio_channels;
	boost::optional<int> _audio_frame_rate;
	Frame _audio_length;
	std::string _name;
	bool _has_subtitles;
	bool _encrypted;
	bool _needs_assets;
	bool _kdm_valid;
	boost::optional<dcp::Standard> _standard;
	bool _three_d;
	std::string _cpl;
};
