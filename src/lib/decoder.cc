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
using std::list;
using boost::shared_ptr;

/** @param f Film.
 *  @param o Options.
 *  @param j Job that we are running within, or 0
 *  @param minimal true to do the bare minimum of work; just run through the content.  Useful for acquiring
 *  accurate frame counts as quickly as possible.  This generates no video or audio output.
 *  @param ignore_length Ignore the content's claimed length when computing progress.
 */
Decoder::Decoder (boost::shared_ptr<Film> f, boost::shared_ptr<const Options> o, Job* j, bool minimal, bool ignore_length)
	: _film (f)
	, _opt (o)
	, _job (j)
	, _minimal (minimal)
	, _ignore_length (ignore_length)
	, _video_frame_index (0)
	, _delay_line (0)
	, _delay_in_bytes (0)
	, _audio_frames_processed (0)
{
	if (_opt->decode_video_frequency != 0 && !_film->length()) {
		throw DecodeError ("cannot do a partial decode if length is unknown");
	}
}

Decoder::~Decoder ()
{
	delete _delay_line;
}

/** Start off a decode processing run */
void
Decoder::process_begin ()
{
	_delay_in_bytes = _film->audio_delay() * audio_sample_rate() * audio_channels() * bytes_per_audio_sample() / 1000;
	delete _delay_line;
	_delay_line = new DelayLine (_delay_in_bytes);

	_audio_frames_processed = 0;
}

/** Finish off a decode processing run */
void
Decoder::process_end ()
{
	if (_delay_in_bytes < 0) {
		uint8_t remainder[-_delay_in_bytes];
		_delay_line->get_remaining (remainder);
		_audio_frames_processed += _delay_in_bytes / (audio_channels() * bytes_per_audio_sample());
		emit_audio (remainder, -_delay_in_bytes);
	}

	/* If we cut the decode off, the audio may be short; push some silence
	   in to get it to the right length.
	*/

	int64_t const video_length_in_audio_frames = ((int64_t) video_frame_index() * audio_sample_rate() / frames_per_second());
	int64_t const audio_short_by_frames = video_length_in_audio_frames - _audio_frames_processed;

	_film->log()->log (
		String::compose ("DCP length is %1 (%2 audio frames); %3 frames of audio processed.",
				 video_frame_index(),
				 video_length_in_audio_frames,
				 _audio_frames_processed)
		);
	
	if (audio_short_by_frames >= 0 && _opt->decode_audio) {

		_film->log()->log (String::compose ("DCP length is %1; %2 frames of audio processed.", video_frame_index(), _audio_frames_processed));
		_film->log()->log (String::compose ("Adding %1 frames of silence to the end.", audio_short_by_frames));

		/* XXX: this is slightly questionable; does memset () give silence with all
		   sample formats?
		*/

		int64_t bytes = audio_short_by_frames * _film->audio_channels() * bytes_per_audio_sample();
		
		int64_t const silence_size = 16 * 1024 * _film->audio_channels() * bytes_per_audio_sample();
		uint8_t silence[silence_size];
		memset (silence, 0, silence_size);
		
		while (bytes) {
			int64_t const t = min (bytes, silence_size);
			emit_audio (silence, t);
			bytes -= t;
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
		if (_job && _film->dcp_length()) {
			_job->set_progress (float (_video_frame_index) / _film->dcp_length().get());
		}
	}

	process_end ();
}

/** Run one pass.  This may or may not generate any actual video / audio data;
 *  some decoders may require several passes to generate a single frame.
 *  @return true if we have finished processing all data; otherwise false.
 */
bool
Decoder::pass ()
{
	if (!_ignore_length && _video_frame_index >= _film->dcp_length()) {
		return true;
	}

	return do_pass ();
}

/** Called by subclasses to tell the world that some audio data is ready
 *  @param data Audio data, in Film::audio_sample_format.
 *  @param size Number of bytes of data.
 */
void
Decoder::process_audio (uint8_t* data, int size)
{
	/* Push into the delay line */
	size = _delay_line->feed (data, size);

	emit_audio (data, size);
}

void
Decoder::emit_audio (uint8_t* data, int size)
{
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

	/* Update the number of audio frames we've pushed to the encoder */
	_audio_frames_processed += audio->frames ();

	Audio (audio);
}

/** Called by subclasses to tell the world that some video data is ready.
 *  We do some post-processing / filtering then emit it for listeners.
 *  @param frame to decode; caller manages memory.
 */
void
Decoder::process_video (AVFrame* frame)
{
	if (_minimal) {
		++_video_frame_index;
		return;
	}

	/* Use Film::length here as our one may be wrong */

	int gap = 0;
	if (_opt->decode_video_frequency != 0) {
		gap = _film->length().get() / _opt->decode_video_frequency;
	}

	if (_opt->decode_video_frequency != 0 && gap != 0 && (_video_frame_index % gap) != 0) {
		++_video_frame_index;
		return;
	}

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
		if (_opt->black_after > 0 && _video_frame_index > _opt->black_after) {
			(*i)->make_black ();
		}
		
		shared_ptr<Subtitle> sub;
		if (_timed_subtitle && _timed_subtitle->displayed_at (double (video_frame_index()) / _film->frames_per_second())) {
			sub = _timed_subtitle->subtitle ();
		}
		
		TIMING ("Decoder emits %1", _video_frame_index);
		Video (*i, _video_frame_index, sub);
		++_video_frame_index;
		_last_image = *i;
		_last_subtitle = sub;
	}
}

void
Decoder::repeat_last_video ()
{
	assert (_last_image);
	Video (_last_image, _video_frame_index, _last_subtitle);
	++_video_frame_index;
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
