/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

/** @file  src/lib/dcp_examiner.h
 *  @brief DCPExaminer class.
 */

#include "video_examiner.h"
#include "audio_examiner.h"

class DCPContent;

class DCPExaminer : public VideoExaminer, public AudioExaminer
{
public:
	DCPExaminer (boost::shared_ptr<const DCPContent>);
	
	boost::optional<float> video_frame_rate () const {
		return _video_frame_rate;
	}
	
	dcp::Size video_size () const {
		return _video_size.get_value_or (dcp::Size (1998, 1080));
	}
	
	ContentTime video_length () const {
		return _video_length;
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

	int audio_channels () const {
		return _audio_channels.get_value_or (0);
	}
	
	ContentTime audio_length () const {
		return _audio_length;
	}
	
	int audio_frame_rate () const {
		return _audio_frame_rate.get_value_or (48000);
	}

	bool kdm_valid () const {
		return _kdm_valid;
	}

private:
	boost::optional<float> _video_frame_rate;
	boost::optional<dcp::Size> _video_size;
	ContentTime _video_length;
	boost::optional<int> _audio_channels;
	boost::optional<int> _audio_frame_rate;
	ContentTime _audio_length;
	std::string _name;
	bool _has_subtitles;
	bool _encrypted;
	bool _kdm_valid;
};
