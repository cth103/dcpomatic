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

using std::min;
using boost::shared_ptr;

Matcher::Matcher (Log* log, int sample_rate, float frames_per_second)
	: AudioVideoProcessor (log)
	, _sample_rate (sample_rate)
	, _frames_per_second (frames_per_second)
	, _video_frames (0)
	, _audio_frames (0)
{

}

void
Matcher::process_video (boost::shared_ptr<Image> i, bool same, boost::shared_ptr<Subtitle> s)
{
	Video (i, same, s);
	_video_frames++;

	_pixel_format = i->pixel_format ();
	_size = i->size ();
}

void
Matcher::process_audio (boost::shared_ptr<AudioBuffers> b)
{
	Audio (b);
	_audio_frames += b->frames ();

	_channels = b->channels ();
}

void
Matcher::process_end ()
{
	if (_audio_frames == 0 || !_pixel_format || !_size || !_channels) {
		/* We won't do anything */
		return;
	}
	
	int64_t audio_short_by_frames = video_frames_to_audio_frames (_video_frames, _sample_rate, _frames_per_second) - _audio_frames;

	_log->log (
		String::compose (
			"Matching processor has seen %1 video frames (which equals %2 audio frames) and %3 audio frames",
			_video_frames,
			video_frames_to_audio_frames (_video_frames, _sample_rate, _frames_per_second),
			_audio_frames
			)
		);
	
	if (audio_short_by_frames < 0) {
		
		_log->log (String::compose ("%1 too many audio frames", -audio_short_by_frames));
		
		/* We have seen more audio than video.  Emit enough black video frames so that we reverse this */
		int const black_video_frames = ceil (-audio_short_by_frames * _frames_per_second / _sample_rate);
		
		_log->log (String::compose ("Emitting %1 frames of black video", black_video_frames));

		shared_ptr<Image> black (new SimpleImage (_pixel_format.get(), _size.get(), false));
		black->make_black ();
		for (int i = 0; i < black_video_frames; ++i) {
			Video (black, i != 0, shared_ptr<Subtitle>());
		}
		
		/* Now recompute our check value */
		audio_short_by_frames = video_frames_to_audio_frames (_video_frames, _sample_rate, _frames_per_second) - _audio_frames;
	}
	
	if (audio_short_by_frames > 0) {
		_log->log (String::compose ("Emitted %1 too few audio frames", audio_short_by_frames));

		/* Do things in half second blocks as I think there may be limits
		   to what FFmpeg (and in particular the resampler) can cope with.
		*/
		int64_t const block = _sample_rate / 2;
		shared_ptr<AudioBuffers> b (new AudioBuffers (_channels.get(), block));
		b->make_silent ();
		
		int64_t to_do = audio_short_by_frames;
		while (to_do > 0) {
			int64_t const this_time = min (to_do, block);
			b->set_frames (this_time);
			Audio (b);
			_audio_frames += b->frames ();
			to_do -= this_time;
		}
	}
}
