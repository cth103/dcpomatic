/*
    Copyright (C) 2016-2021 Carl Hetherington <cth@carlh.net>

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


#include "audio_analyser.h"
#include "dcp_text_track.h"
#include "dcpomatic_time.h"
#include "player_text.h"
#include "signaller.h"
#include "text_type.h"
#include "weak_film.h"
#include <boost/atomic.hpp>
#include <boost/signals2.hpp>
#include <string>
#include <vector>


class Film;
class Writer;


class Hints : public Signaller, public ExceptionStore, public WeakConstFilm
{
public:
	explicit Hints(std::weak_ptr<const Film> film);
	~Hints();

	void start();

	boost::signals2::signal<void (std::string)> Hint;
	boost::signals2::signal<void (std::string)> Progress;
	boost::signals2::signal<void (void)> Pulse;
	boost::signals2::signal<void (void)> Finished;

	/* For tests only */
	void join();
	void disable_audio_analysis() {
		_disable_audio_analysis = true;
	}

private:
	friend struct hint_subtitle_too_early;

	void thread();
	void scan_content(std::shared_ptr<const Film> film);
	void hint(std::string h);
	void audio(std::shared_ptr<AudioBuffers> audio, dcpomatic::DCPTime time);
	void text(PlayerText text, TextType type, boost::optional<DCPTextTrack> track, dcpomatic::DCPTimePeriod period);
	void closed_caption(PlayerText text, dcpomatic::DCPTimePeriod period);
	void open_subtitle(PlayerText text, dcpomatic::DCPTimePeriod period);

	void check_certificates();
	void check_interop();
	void check_video_encoding();
	void check_big_font_files();
	void check_few_audio_channels();
	void check_upmixers();
	void check_incorrect_container();
	void check_unusual_container();
	void check_high_video_bit_rate();
	void check_frame_rate();
	void check_4k_3d();
	void check_speed_up();
	void check_vob();
	void check_3d_in_2d();
	bool check_loudness();
	void check_ffec_and_ffmc_in_smpte_feature();
	void check_out_of_range_markers();
	void check_subtitle_languages();
	void check_audio_language();
	void check_8_or_16_audio_channels();
	void check_video_alpha();

	boost::thread _thread;
	/** This is used to make a partial DCP containing only the subtitles and closed captions that
	 *  our final DCP will have.  This means we can see how big the files will be and warn if they
	 *  will be too big.
	 */
	std::shared_ptr<Writer> _writer;

	AudioAnalyser _analyser;

	bool _long_ccap = false;
	bool _overlap_ccap = false;
	bool _too_many_ccap_lines = false;
	boost::optional<dcpomatic::DCPTimePeriod> _last_ccap;

	bool _early_subtitle = false;
	bool _short_subtitle = false;
	bool _subtitles_too_close = false;
	bool _too_many_subtitle_lines = false;
	bool _long_subtitle = false;
	bool _very_long_subtitle = false;
	boost::optional<dcpomatic::DCPTimePeriod> _last_subtitle;

	boost::atomic<bool> _stop;

	bool _disable_audio_analysis = false;
};
