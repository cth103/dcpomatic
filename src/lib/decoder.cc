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

/** @param f Film.
 *  @param o Options.
 *  @param j Job that we are running within, or 0
 *  @param minimal true to do the bare minimum of work; just run through the content.  Useful for acquiring
 *  accurate frame counts as quickly as possible.  This generates no video or audio output.
 */
Decoder::Decoder (boost::shared_ptr<Film> f, boost::shared_ptr<const Options> o, Job* j)
	: _film (f)
	, _opt (o)
	, _job (j)
	, _video_frame (0)
	, _audio_frame (0)
	, _delay_line (0)
	, _delay_in_frames (0)
{
	
}

Decoder::~Decoder ()
{
	delete _delay_line;
}

/** Start off a decode processing run.  This should only be called once on
 *  a given Decoder object.
 */
void
Decoder::process_begin ()
{
	_delay_in_frames = _film->audio_delay() * audio_sample_rate() / 1000;
	_delay_line = new DelayLine (audio_channels(), _delay_in_frames);
}

/** Finish off a decode processing run */
void
Decoder::process_end ()
{
	if (_delay_in_frames < 0 && _opt->decode_audio && audio_channels()) {
		shared_ptr<AudioBuffers> b (new AudioBuffers (audio_channels(), -_delay_in_frames));
		b->make_silent ();
		emit_audio (b);
	}

	if (_opt->decode_audio && audio_channels()) {

		/* Ensure that our video and audio emissions are the same length */

		int64_t audio_short_by_frames = video_frames_to_audio_frames (_video_frame, audio_sample_rate(), frames_per_second()) - _audio_frame;

		_film->log()->log (
			String::compose (
				"Decoder has emitted %1 video frames (which equals %2 audio frames) and %3 audio frames",
				_video_frame,
				video_frames_to_audio_frames (_video_frame, audio_sample_rate(), frames_per_second()),
				_audio_frame
				)
			);

		if (audio_short_by_frames < 0) {

			_film->log()->log (String::compose ("Emitted %1 too many audio frames", -audio_short_by_frames));
			
			/* We have emitted more audio than video.  Emit enough black video frames so that we reverse this */
			int const black_video_frames = ceil (-audio_short_by_frames * frames_per_second() / audio_sample_rate());

			_film->log()->log (String::compose ("Emitting %1 frames of black video", black_video_frames));

			shared_ptr<Image> black (new CompactImage (pixel_format(), native_size()));
			black->make_black ();
			for (int i = 0; i < black_video_frames; ++i) {
				emit_video (black, shared_ptr<Subtitle> ());
			}

			/* Now recompute our check value */
			audio_short_by_frames = video_frames_to_audio_frames (_video_frame, audio_sample_rate(), frames_per_second()) - _audio_frame;
		}
	
		if (audio_short_by_frames > 0) {
			_film->log()->log (String::compose ("Emitted %1 too few audio frames", audio_short_by_frames));
			shared_ptr<AudioBuffers> b (new AudioBuffers (audio_channels(), audio_short_by_frames));
			b->make_silent ();
			emit_audio (b);
		}
	}
}

/** Start decoding */
void
Decoder::go ()
{
	process_begin ();

	if (_job && !_film->dcp_length()) {
		_job->set_progress_unknown ();
	}

	while (pass () == false) {
		if (_job) {
			_job->set_progress (float (_video_frame) / _film->length().get());
		}
	}

	process_end ();
}

/** Called by subclasses to tell the world that some audio data is ready
 *  @param data Audio data, in Film::audio_sample_format.
 *  @param size Number of bytes of data.
 */
void
Decoder::process_audio (uint8_t* data, int size)
{
	/* XXX: could this be removed? */
	if (size == 0) {
		return;
	}
	
	assert (_film->audio_channels());
	assert (bytes_per_audio_sample());
	
	/* Deinterleave and convert to float */

	assert ((size % (bytes_per_audio_sample() * audio_channels())) == 0);

	int const total_samples = size / bytes_per_audio_sample();
	int const frames = total_samples / _film->audio_channels();
	shared_ptr<AudioBuffers> audio (new AudioBuffers (audio_channels(), frames));

	switch (audio_sample_format()) {
	case AV_SAMPLE_FMT_S16:
	{
		int16_t* p = (int16_t *) data;
		int sample = 0;
		int channel = 0;
		for (int i = 0; i < total_samples; ++i) {
			audio->data(channel)[sample] = float(*p++) / (1 << 15);

			++channel;
			if (channel == _film->audio_channels()) {
				channel = 0;
				++sample;
			}
		}
	}
	break;

	case AV_SAMPLE_FMT_S32:
	{
		int32_t* p = (int32_t *) data;
		int sample = 0;
		int channel = 0;
		for (int i = 0; i < total_samples; ++i) {
			audio->data(channel)[sample] = float(*p++) / (1 << 31);

			++channel;
			if (channel == _film->audio_channels()) {
				channel = 0;
				++sample;
			}
		}
	}

	case AV_SAMPLE_FMT_FLTP:
	{
		float* p = reinterpret_cast<float*> (data);
		for (int i = 0; i < _film->audio_channels(); ++i) {
			memcpy (audio->data(i), p, frames * sizeof(float));
			p += frames;
		}
	}
	break;

	default:
		assert (false);
	}

	/* Maybe apply gain */
	if (_film->audio_gain() != 0) {
		float const linear_gain = pow (10, _film->audio_gain() / 20);
		for (int i = 0; i < _film->audio_channels(); ++i) {
			for (int j = 0; j < frames; ++j) {
				audio->data(i)[j] *= linear_gain;
			}
		}
	}

	_delay_line->feed (audio);
	emit_audio (audio);
}

/** Called by subclasses to tell the world that some video data is ready.
 *  We do some post-processing / filtering then emit it for listeners.
 *  @param frame to decode; caller manages memory.
 */
void
Decoder::process_video (AVFrame* frame)
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
	Video (image, _video_frame, sub);
	++_video_frame;

	_last_image = image;
	_last_subtitle = sub;
}

void
Decoder::emit_audio (shared_ptr<AudioBuffers> audio)
{
	Audio (audio, _audio_frame);
	_audio_frame += audio->frames ();
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

int
Decoder::bytes_per_audio_sample () const
{
	return av_get_bytes_per_sample (audio_sample_format ());
}
