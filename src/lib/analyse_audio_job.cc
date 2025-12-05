/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include "analyse_audio_job.h"
#include "audio_analysis.h"
#include "dcpomatic_log.h"
#include "film.h"
#include "filter.h"
#include "player.h"
#include "playlist.h"
#include "config.h"
#include <iostream>

#include "i18n.h"


using std::cout;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::max;
using std::min;
using std::shared_ptr;
using std::string;
using std::vector;
using namespace dcpomatic;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif


/** @param whole_film true to analyse the whole film' audio (i.e. start from time 0 and use processors), false
 *  to analyse just the single piece of content in the playlist (i.e. start from Playlist::start() and do not
 *  use processors).
 */
AnalyseAudioJob::AnalyseAudioJob(shared_ptr<const Film> film, shared_ptr<const Playlist> playlist, bool whole_film)
	: Job(film)
	, _analyser(film, playlist, whole_film, boost::bind(&Job::set_progress, this, _1, false))
	, _playlist(playlist)
	, _path(film->audio_analysis_path(playlist))
	, _whole_film(whole_film)
{
	LOG_DEBUG_AUDIO_ANALYSIS("AnalyseAudioJob::AnalyseAudioJob");
}


AnalyseAudioJob::~AnalyseAudioJob()
{
	stop_thread();
}


string
AnalyseAudioJob::name() const
{
	return _("Analysing audio");
}


string
AnalyseAudioJob::json_name() const
{
	return N_("analyse_audio");
}


void
AnalyseAudioJob::run()
{
	LOG_DEBUG_AUDIO_ANALYSIS("AnalyseAudioJob::run");

	auto player = make_shared<Player>(_film, _playlist, false);
	player->set_ignore_video();
	player->set_ignore_text();
	player->set_fast();
	player->set_play_referenced();
	player->Audio.connect(bind(&AudioAnalyser::analyse, &_analyser, _1, _2));
	if (!_whole_film) {
		player->set_disable_audio_processor();
	}

	bool has_any_audio = false;
	for (auto c: _playlist->content()) {
		if (c->audio) {
			has_any_audio = true;
		}
	}

	if (has_any_audio) {
		player->seek(_analyser.start(), true);
		while (!player->pass()) {}
	}

	LOG_DEBUG_AUDIO_ANALYSIS("Loop complete");

	_analyser.finish();
	auto analysis = _analyser.get();
	analysis.write(_path);

	LOG_DEBUG_AUDIO_ANALYSIS("Job finished");
	set_progress(1);
	set_state(FINISHED_OK);
}
