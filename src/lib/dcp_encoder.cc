/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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

/** @file  src/dcp_encoder.cc
 *  @brief A class which takes a Film and some Options, then uses those to encode the film into a DCP.
 *
 *  A decoder is selected according to the content type, and the encoder can be specified
 *  as a parameter to the constructor.
 */

#include "dcp_encoder.h"
#include "j2k_encoder.h"
#include "film.h"
#include "video_decoder.h"
#include "audio_decoder.h"
#include "player.h"
#include "job.h"
#include "writer.h"
#include "compose.hpp"
#include "referenced_reel_asset.h"
#include "text_content.h"
#include "player_video.h"
#include <boost/signals2.hpp>
#include <boost/foreach.hpp>
#include <iostream>

#include "i18n.h"

using std::string;
using std::cout;
using std::list;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using namespace dcpomatic;

/** Construct a DCP encoder.
 *  @param film Film that we are encoding.
 *  @param job Job that this encoder is being used in.
 */
DCPEncoder::DCPEncoder (shared_ptr<const Film> film, weak_ptr<Job> job)
	: Encoder (film, job)
	, _finishing (false)
	, _non_burnt_subtitles (false)
{
	_player_video_connection = _player->Video.connect (bind (&DCPEncoder::video, this, _1, _2));
	_player_audio_connection = _player->Audio.connect (bind (&DCPEncoder::audio, this, _1, _2));
	_player_text_connection = _player->Text.connect (bind (&DCPEncoder::text, this, _1, _2, _3, _4));
	_player_atmos_connection = _player->Atmos.connect (bind (&DCPEncoder::atmos, this, _1, _2, _3));

	BOOST_FOREACH (shared_ptr<const Content> c, film->content ()) {
		BOOST_FOREACH (shared_ptr<TextContent> i, c->text) {
			if (i->use() && !i->burn()) {
				_non_burnt_subtitles = true;
			}
		}
	}
}

DCPEncoder::~DCPEncoder ()
{
	/* We must stop receiving more video data before we die */
	_player_video_connection.release ();
	_player_audio_connection.release ();
	_player_text_connection.release ();
	_player_atmos_connection.release ();
}

void
DCPEncoder::go ()
{
	_writer.reset (new Writer (_film, _job));
	_writer->start ();

	_j2k_encoder.reset (new J2KEncoder (_film, _writer));
	_j2k_encoder->begin ();

	{
		shared_ptr<Job> job = _job.lock ();
		DCPOMATIC_ASSERT (job);
		job->sub (_("Encoding"));
	}

	if (_non_burnt_subtitles) {
		list<shared_ptr<Font> > fonts = _player->get_subtitle_fonts ();

		if (fonts.size() > 1 && _film->interop()) {
			/* Interop will ignore second and subsequent <LoadFont>s so don't even
			   write them as they upset some validators.
			*/
			shared_ptr<Font> first = fonts.front ();
			fonts.clear ();
			fonts.push_back (first);
		}

		_writer->write (fonts);
	}

	while (!_player->pass ()) {}

	BOOST_FOREACH (ReferencedReelAsset i, _player->get_reel_assets ()) {
		_writer->write (i);
	}

	_finishing = true;
	_j2k_encoder->end ();
	_writer->finish ();
}

void
DCPEncoder::video (shared_ptr<PlayerVideo> data, DCPTime time)
{
	if (!_film->three_d()) {
		if (data->eyes() == EYES_LEFT) {
			/* Use left-eye images for both eyes... */
			data->set_eyes (EYES_BOTH);
		} else if (data->eyes() == EYES_RIGHT) {
			/* ...and discard the right */
			return;
		}
	}

	_j2k_encoder->encode (data, time);
}

void
DCPEncoder::audio (shared_ptr<AudioBuffers> data, DCPTime time)
{
	_writer->write (data, time);

	shared_ptr<Job> job = _job.lock ();
	DCPOMATIC_ASSERT (job);
	job->set_progress (float(time.get()) / _film->length().get());
}

void
DCPEncoder::text (PlayerText data, TextType type, optional<DCPTextTrack> track, DCPTimePeriod period)
{
	if (type == TEXT_CLOSED_CAPTION || _non_burnt_subtitles) {
		_writer->write (data, type, track, period);
	}
}


void
DCPEncoder::atmos (shared_ptr<const dcp::AtmosFrame> data, DCPTime time, AtmosMetadata metadata)
{
	_writer->write (data, time, metadata);
}


optional<float>
DCPEncoder::current_rate () const
{
	if (!_j2k_encoder) {
		return optional<float>();
	}

	return _j2k_encoder->current_encoding_rate ();
}

Frame
DCPEncoder::frames_done () const
{
	if (!_j2k_encoder) {
		return 0;
	}

	return _j2k_encoder->video_frames_enqueued ();
}
