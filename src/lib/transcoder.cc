/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

/** @file  src/transcoder.cc
 *  @brief A class which takes a Film and some Options, then uses those to transcode the film.
 *
 *  A decoder is selected according to the content type, and the encoder can be specified
 *  as a parameter to the constructor.
 */

#include "transcoder.h"
#include "encoder.h"
#include "film.h"
#include "video_decoder.h"
#include "audio_decoder.h"
#include "player.h"
#include "job.h"
#include "writer.h"
#include "subtitle_content.h"
#include <boost/signals2.hpp>
#include <boost/foreach.hpp>
#include <iostream>

using std::string;
using std::cout;
using std::list;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;

/** Construct a transcoder.
 *  @param f Film that we are transcoding.
 *  @param j Job that this transcoder is being used in.
 */
Transcoder::Transcoder (shared_ptr<const Film> film, shared_ptr<Job> j)
	: _film (film)
	, _player (new Player (film, film->playlist ()))
	, _writer (new Writer (film, j))
	, _encoder (new Encoder (film, j, _writer))
	, _finishing (false)
{

}

void
Transcoder::go ()
{
	_encoder->begin ();

	DCPTime const frame = DCPTime::from_frames (1, _film->video_frame_rate ());
	DCPTime const length = _film->length ();

	int burnt_subtitles = 0;
	int non_burnt_subtitles = 0;
	BOOST_FOREACH (shared_ptr<const Content> c, _film->content ()) {
		shared_ptr<const SubtitleContent> sc = dynamic_pointer_cast<const SubtitleContent> (c);
		if (sc && sc->use_subtitles()) {
			if (sc->burn_subtitles()) {
				++burnt_subtitles;
			} else {
				++non_burnt_subtitles;
			}
		}
	}

	if (non_burnt_subtitles) {
		_writer->write (_player->get_subtitle_fonts ());
	}

	for (DCPTime t; t < length; t += frame) {
		list<shared_ptr<PlayerVideo> > v = _player->get_video (t, true);
		for (list<shared_ptr<PlayerVideo> >::const_iterator i = v.begin(); i != v.end(); ++i) {
			_encoder->enqueue (*i);
		}
		_writer->write (_player->get_audio (t, frame, true));
		if (non_burnt_subtitles) {
			_writer->write (_player->get_subtitles (t, frame, true, false));
		}
	}

	_finishing = true;
	_encoder->end ();
	_writer->finish ();
}

float
Transcoder::current_encoding_rate () const
{
	return _encoder->current_encoding_rate ();
}

int
Transcoder::video_frames_out () const
{
	return _encoder->video_frames_out ();
}
