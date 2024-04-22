/*
    Copyright (C) 2019-2021 Carl Hetherington <cth@carlh.net>

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


#include "dcp_text_track.h"
#include "dcpomatic_time.h"
#include "film_encoder.h"
#include "player_text.h"


namespace dcp {
	class SubtitleAsset;
}


class Film;


/** @class SubtitleFilmEncoder.
 *  @brief An `encoder' which extracts a film's subtitles to DCP XML format.
 */
class SubtitleFilmEncoder : public FilmEncoder
{
public:
	SubtitleFilmEncoder(std::shared_ptr<const Film> film, std::shared_ptr<Job> job, boost::filesystem::path output, std::string initial_name, bool split_reels, bool include_font);

	void go () override;

	/** @return the number of frames that are done */
	Frame frames_done () const override;

	bool finishing () const override {
		return false;
	}

private:
	void text (PlayerText subs, TextType type, boost::optional<DCPTextTrack> track, dcpomatic::DCPTimePeriod period);

	std::vector<std::pair<std::shared_ptr<dcp::SubtitleAsset>, boost::filesystem::path>> _assets;
	std::vector<dcpomatic::DCPTimePeriod> _reels;
	bool _split_reels;
	bool _include_font;
	int _reel_index;
	boost::optional<dcpomatic::DCPTime> _last;
	dcpomatic::DCPTime _length;
	dcp::ArrayData _default_font;
};
