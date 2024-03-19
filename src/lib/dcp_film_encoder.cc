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


/** @file  src/dcp_film_encoder.cc
 *  @brief A class which takes a Film and some Options, then uses those to encode the film into a DCP.
 *
 *  A decoder is selected according to the content type, and the encoder can be specified
 *  as a parameter to the constructor.
 */


#include "audio_decoder.h"
#include "compose.hpp"
#include "dcp_film_encoder.h"
#include "film.h"
#include "j2k_encoder.h"
#include "job.h"
#include "player.h"
#include "player_video.h"
#include "referenced_reel_asset.h"
#include "text_content.h"
#include "video_decoder.h"
#include "writer.h"
#include <boost/signals2.hpp>
#include <iostream>

#include "i18n.h"


using std::cout;
using std::dynamic_pointer_cast;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using std::weak_ptr;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;


/** Construct a DCP encoder.
 *  @param film Film that we are encoding.
 *  @param job Job that this encoder is being used in.
 */
DCPFilmEncoder::DCPFilmEncoder(shared_ptr<const Film> film, weak_ptr<Job> job)
	: FilmEncoder(film, job)
	, _writer(film, job)
	, _encoder(new J2KEncoder(film, _writer))
	, _finishing (false)
	, _non_burnt_subtitles (false)
{
	_player_video_connection = _player.Video.connect(bind(&DCPFilmEncoder::video, this, _1, _2));
	_player_audio_connection = _player.Audio.connect(bind(&DCPFilmEncoder::audio, this, _1, _2));
	_player_text_connection = _player.Text.connect(bind(&DCPFilmEncoder::text, this, _1, _2, _3, _4));
	_player_atmos_connection = _player.Atmos.connect(bind(&DCPFilmEncoder::atmos, this, _1, _2, _3));

	for (auto c: film->content ()) {
		for (auto i: c->text) {
			if (i->use() && !i->burn()) {
				_non_burnt_subtitles = true;
			}
		}
	}
}

DCPFilmEncoder::~DCPFilmEncoder()
{
	/* We must stop receiving more video data before we die */
	_player_video_connection.release ();
	_player_audio_connection.release ();
	_player_text_connection.release ();
	_player_atmos_connection.release ();
}

void
DCPFilmEncoder::go()
{
	_writer.start();
	_encoder->begin();

	{
		auto job = _job.lock ();
		DCPOMATIC_ASSERT (job);
		job->sub (_("Encoding"));
	}

	if (_non_burnt_subtitles) {
		_writer.write(_player.get_subtitle_fonts());
	}

	while (!_player.pass()) {}

	for (auto i: get_referenced_reel_assets(_film, _film->playlist())) {
		_writer.write(i);
	}

	_finishing = true;
	_encoder->end();
	_writer.finish(_film->dir(_film->dcp_name()));
}


void
DCPFilmEncoder::pause()
{
	_encoder->pause();
}


void
DCPFilmEncoder::resume()
{
	_encoder->resume();
}

void
DCPFilmEncoder::video(shared_ptr<PlayerVideo> data, DCPTime time)
{
	_encoder->encode(data, time);
}

void
DCPFilmEncoder::audio(shared_ptr<AudioBuffers> data, DCPTime time)
{
	_writer.write(data, time);

	auto job = _job.lock ();
	DCPOMATIC_ASSERT (job);
	job->set_progress (float(time.get()) / _film->length().get());
}

void
DCPFilmEncoder::text(PlayerText data, TextType type, optional<DCPTextTrack> track, DCPTimePeriod period)
{
	if (type == TextType::CLOSED_CAPTION || _non_burnt_subtitles) {
		_writer.write(data, type, track, period);
	}
}


void
DCPFilmEncoder::atmos(shared_ptr<const dcp::AtmosFrame> data, DCPTime time, AtmosMetadata metadata)
{
	_writer.write(data, time, metadata);
}


optional<float>
DCPFilmEncoder::current_rate() const
{
	return _encoder->current_encoding_rate();
}

Frame
DCPFilmEncoder::frames_done() const
{
	return _encoder->video_frames_enqueued();
}
