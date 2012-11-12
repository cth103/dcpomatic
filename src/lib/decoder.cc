/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

/** @file  src/decoder.cc
 *  @brief Parent class for decoders of content.
 */

#include <iostream>
#include <stdint.h>
#include <boost/lexical_cast.hpp>
#include "film.h"
#include "format.h"
#include "job.h"
#include "options.h"
#include "exceptions.h"
#include "image.h"
#include "util.h"
#include "log.h"
#include "decoder.h"
#include "delay_line.h"
#include "subtitle.h"
#include "filter_graph.h"

using std::string;
using std::stringstream;
using std::min;
using std::pair;
using std::list;
using boost::shared_ptr;
using boost::optional;

/** @param f Film.
 *  @param o Options.
 *  @param j Job that we are running within, or 0
 */
Decoder::Decoder (boost::shared_ptr<Film> f, boost::shared_ptr<const Options> o, Job* j)
	: _film (f)
	, _opt (o)
	, _job (j)
	, _video_frame (0)
{
	
}

/** Start decoding */
void
Decoder::go ()
{
	if (_job && !_film->dcp_length()) {
		_job->set_progress_unknown ();
	}

	while (pass () == false) {
		if (_job && _film->dcp_length()) {
			_job->set_progress (float (_video_frame) / _film->length().get());
		}
	}
}

/** Called to tell the world that some audio data is ready
 *  @param audio Audio data.
 */
void
Decoder::process_audio (shared_ptr<AudioBuffers> audio)
{
	/* Maybe apply gain */
	if (_film->audio_gain() != 0) {
		float const linear_gain = pow (10, _film->audio_gain() / 20);
		for (int i = 0; i < audio->channels(); ++i) {
			for (int j = 0; j < audio->frames(); ++j) {
				audio->data(i)[j] *= linear_gain;
			}
		}
	}

	Audio (audio);
}

/** Called by subclasses to tell the world that some video data is ready.
 *  We do some post-processing / filtering then emit it for listeners.
 *  @param frame to decode; caller manages memory.
 */
void
Decoder::process_video (AVFrame const * frame)
{
	shared_ptr<FilterGraph> graph;

	list<shared_ptr<FilterGraph> >::iterator i = _filter_graphs.begin();
	while (i != _filter_graphs.end() && !(*i)->can_process (Size (frame->width, frame->height), (AVPixelFormat) frame->format)) {
		++i;
	}

	if (i == _filter_graphs.end ()) {
		graph.reset (new FilterGraph (_film, this, _opt->apply_crop, Size (frame->width, frame->height), (AVPixelFormat) frame->format));
		_filter_graphs.push_back (graph);
		_film->log()->log (String::compose ("New graph for %1x%2, pixel format %3", frame->width, frame->height, frame->format));
	} else {
		graph = *i;
	}

	list<shared_ptr<Image> > images = graph->process (frame);

	for (list<shared_ptr<Image> >::iterator i = images.begin(); i != images.end(); ++i) {
		shared_ptr<Subtitle> sub;
		if (_timed_subtitle && _timed_subtitle->displayed_at (double (video_frame()) / _film->frames_per_second())) {
			sub = _timed_subtitle->subtitle ();
		}

		emit_video (*i, sub);
	}
}

void
Decoder::repeat_last_video ()
{
	if (!_last_image) {
		_last_image.reset (new CompactImage (pixel_format(), native_size()));
		_last_image->make_black ();
	}

	emit_video (_last_image, _last_subtitle);
}

void
Decoder::emit_video (shared_ptr<Image> image, shared_ptr<Subtitle> sub)
{
	TIMING ("Decoder emits %1", _video_frame);
	Video (image, sub);
	++_video_frame;

	_last_image = image;
	_last_subtitle = sub;
}

void
Decoder::process_subtitle (shared_ptr<TimedSubtitle> s)
{
	_timed_subtitle = s;
	
	if (_timed_subtitle && _opt->apply_crop) {
		Position const p = _timed_subtitle->subtitle()->position ();
		_timed_subtitle->subtitle()->set_position (Position (p.x - _film->crop().left, p.y - _film->crop().top));
	}
}

void
Decoder::set_audio_stream (optional<AudioStream> s)
{
	_audio_stream = s;
}

void
Decoder::set_subtitle_stream (optional<SubtitleStream> s)
{
	_subtitle_stream = s;
}
