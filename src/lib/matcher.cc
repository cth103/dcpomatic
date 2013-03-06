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

#include "matcher.h"
#include "image.h"
#include "log.h"

#include "i18n.h"

using std::min;
using std::cout;
using std::list;
using boost::shared_ptr;

Matcher::Matcher (Log* log, int sample_rate, float frames_per_second)
	: Processor (log)
	, _sample_rate (sample_rate)
	, _frames_per_second (frames_per_second)
	, _video_frames (0)
	, _audio_frames (0)
{

}

void
Matcher::process_video (boost::shared_ptr<Image> image, bool same, boost::shared_ptr<Subtitle> sub, double t)
{
	_pixel_format = image->pixel_format ();
	_size = image->size ();

	_log->log(String::compose("Matcher video @ %1 (same=%2)", t, same));

	if (!_first_input) {
		_first_input = t;
	}
	
	if (_audio_frames == 0 && _pending_audio.empty ()) {
		/* No audio yet; we must postpone this frame until we have some */
		_pending_video.push_back (VideoRecord (image, same, sub, t));
	} else if (!_pending_audio.empty() && _pending_video.empty ()) {
		/* First video since we got audio */
		_pending_video.push_back (VideoRecord (image, same, sub, t));
		fix_start ();
	} else {
		/* Normal running */

		/* Difference between where this video is and where it should be */
		double const delta = t - _first_input.get() - _video_frames / _frames_per_second;
		double const one_frame = 1 / _frames_per_second;

		if (delta > one_frame) {
			/* Insert frames to make up the difference */
			int const extra = rint (delta / one_frame);
			for (int i = 0; i < extra; ++i) {
				repeat_last_video ();
				_log->log (String::compose ("Extra video frame inserted at %1s", _video_frames / _frames_per_second));
			}
		}

		if (delta > -one_frame) {
			Video (image, same, sub);
			++_video_frames;
		} else {
			/* We are omitting a frame to keep things right */
			_log->log (String::compose ("Frame removed at %1s", t));
		}
	}
		
	_last_image = image;
	_last_subtitle = sub;
}

void
Matcher::process_audio (boost::shared_ptr<AudioBuffers> b, double t)
{
	_channels = b->channels ();
	
	if (!_first_input) {
		_first_input = t;
	}
	
	if (_video_frames == 0 && _pending_video.empty ()) {
		/* No video yet; we must postpone these data until we have some */
		_pending_audio.push_back (AudioRecord (b, t));
	} else if (!_pending_video.empty() && _pending_audio.empty ()) {
		/* First audio since we got video */
		_pending_audio.push_back (AudioRecord (b, t));
		fix_start ();
	} else {
		/* Normal running.  We assume audio time stamps are consecutive */
		Audio (b);
		_audio_frames += b->frames ();
	}
}

void
Matcher::process_end ()
{
	if (_audio_frames == 0 || !_pixel_format || !_size || !_channels) {
		/* We won't do anything */
		return;
	}
	
	match ((double (_audio_frames) / _sample_rate) - (double (_video_frames) / _frames_per_second));
}

void
Matcher::fix_start ()
{
	assert (!_pending_video.empty ());
	assert (!_pending_audio.empty ());

	_log->log (String::compose ("Fixing start; video at %1, audio at %2", _pending_video.front().time, _pending_audio.front().time));

	match (_pending_video.front().time - _pending_audio.front().time);

	for (list<VideoRecord>::iterator i = _pending_video.begin(); i != _pending_video.end(); ++i) {
		process_video (i->image, i->same, i->subtitle, i->time);
	}

	for (list<AudioRecord>::iterator i = _pending_audio.begin(); i != _pending_audio.end(); ++i) {
		process_audio (i->audio, i->time);
	}
	
	_pending_video.clear ();
	_pending_audio.clear ();
}

void
Matcher::match (double extra_video_needed)
{
	if (extra_video_needed > 0) {

		/* Emit black video frames */
		
		int const black_video_frames = ceil (extra_video_needed * _frames_per_second);

		_log->log (String::compose (N_("Emitting %1 frames of black video"), black_video_frames));

		shared_ptr<Image> black (new SimpleImage (_pixel_format.get(), _size.get(), true));
		black->make_black ();
		for (int i = 0; i < black_video_frames; ++i) {
			Video (black, i != 0, shared_ptr<Subtitle>());
			++_video_frames;
		}

		extra_video_needed -= black_video_frames / _frames_per_second;
	}

	if (extra_video_needed < 0) {
		
		/* Emit silence */
		
		int64_t to_do = -extra_video_needed * _sample_rate;
		_log->log (String::compose (N_("Emitted %1 frames of silence"), to_do));

		/* Do things in half second blocks as I think there may be limits
		   to what FFmpeg (and in particular the resampler) can cope with.
		*/
		int64_t const block = _sample_rate / 2;
		shared_ptr<AudioBuffers> b (new AudioBuffers (_channels.get(), block));
		b->make_silent ();
		
		while (to_do > 0) {
			int64_t const this_time = min (to_do, block);
			b->set_frames (this_time);
			Audio (b);
			_audio_frames += b->frames ();
			to_do -= this_time;
		}
	}
}

void
Matcher::repeat_last_video ()
{
	if (!_last_image) {
		_last_image.reset (new SimpleImage (_pixel_format.get(), _size.get(), true));
		_last_image->make_black ();
	}

	Video (_last_image, true, _last_subtitle);
	++_video_frames;
}

