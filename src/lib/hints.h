/*
    Copyright (C) 2016-2020 Carl Hetherington <cth@carlh.net>

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

#include "signaller.h"
#include "player_text.h"
#include "types.h"
#include "dcp_text_track.h"
#include "dcpomatic_time.h"
#include <boost/weak_ptr.hpp>
#include <boost/signals2.hpp>
#include <boost/atomic.hpp>
#include <vector>
#include <string>

class Film;

class Hints : public Signaller, public ExceptionStore
{
public:
	explicit Hints (boost::weak_ptr<const Film> film);
	~Hints ();

	void start ();

	boost::signals2::signal<void (std::string)> Hint;
	boost::signals2::signal<void (std::string)> Progress;
	boost::signals2::signal<void (void)> Pulse;
	boost::signals2::signal<void (void)> Finished;

	/* For tests only */
	void join ();

private:
	friend struct hint_subtitle_too_early;

	void thread ();
	void hint (std::string h);
	void text (PlayerText text, TextType type, dcpomatic::DCPTimePeriod period);
	void closed_caption (PlayerText text, dcpomatic::DCPTimePeriod period);
	void open_subtitle (PlayerText text, dcpomatic::DCPTimePeriod period);
	boost::shared_ptr<const Film> film () const;

	void check_big_font_files ();
	void check_few_audio_channels ();
	void check_upmixers ();
	void check_incorrect_container ();
	void check_unusual_container ();
	void check_high_j2k_bandwidth ();
	void check_frame_rate ();
	void check_speed_up ();
	void check_vob ();
	void check_3d_in_2d ();
	void check_loudness ();
	void check_ffec_and_ffmc_in_smpte_feature ();

	boost::weak_ptr<const Film> _film;
	boost::thread _thread;

	bool _long_ccap;
	bool _overlap_ccap;
	bool _too_many_ccap_lines;
	boost::optional<dcpomatic::DCPTimePeriod> _last_ccap;

	bool _early_subtitle;
	bool _short_subtitle;
	bool _subtitles_too_close;
	bool _too_many_subtitle_lines;
	bool _long_subtitle;
	boost::optional<dcpomatic::DCPTimePeriod> _last_subtitle;

	boost::atomic<bool> _stop;
};
