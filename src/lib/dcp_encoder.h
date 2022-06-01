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


#include "atmos_metadata.h"
#include "dcp_text_track.h"
#include "dcpomatic_time.h"
#include "encoder.h"
#include "player_text.h"
#include "types.h"
#include <dcp/atmos_frame.h>


class AudioBuffers;
class Film;
class J2KEncoder;
class Job;
class Player;
class PlayerVideo;
class Writer;


/** @class DCPEncoder */
class DCPEncoder : public Encoder
{
public:
	DCPEncoder (std::shared_ptr<const Film> film, std::weak_ptr<Job> job);
	~DCPEncoder ();

	void go () override;

	boost::optional<float> current_rate () const override;
	Frame frames_done () const override;

	/** @return true if we are in the process of calling Encoder::process_end */
	bool finishing () const override {
		return _finishing;
	}

private:

	void video (std::shared_ptr<PlayerVideo>, dcpomatic::DCPTime);
	void audio (std::shared_ptr<AudioBuffers>, dcpomatic::DCPTime);
	void text (PlayerText, TextType, boost::optional<DCPTextTrack>, dcpomatic::DCPTimePeriod);
	void atmos (std::shared_ptr<const dcp::AtmosFrame>, dcpomatic::DCPTime, AtmosMetadata metadata);

	std::shared_ptr<Writer> _writer;
	std::shared_ptr<J2KEncoder> _j2k_encoder;
	bool _finishing;
	bool _non_burnt_subtitles;

	boost::signals2::scoped_connection _player_video_connection;
	boost::signals2::scoped_connection _player_audio_connection;
	boost::signals2::scoped_connection _player_text_connection;
	boost::signals2::scoped_connection _player_atmos_connection;
};
