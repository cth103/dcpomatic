/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#include <boost/shared_ptr.hpp>
#include "trimmer.h"

using std::cout;
using std::max;
using boost::shared_ptr;

/** @param audio_sample_rate Audio sampling rate, or 0 if there is no audio */
Trimmer::Trimmer (
	shared_ptr<Log> log,
	int video_trim_start,
	int video_trim_end, int video_length,
	int audio_sample_rate,
	float frames_per_second,
	int dcp_frames_per_second
	)
	: AudioVideoProcessor (log)
	, _video_start (video_trim_start)
	, _video_end (video_length - video_trim_end)
	, _video_in (0)
	, _audio_in (0)
{
	FrameRateConversion frc (frames_per_second, dcp_frames_per_second);

	if (frc.skip) {
		_video_start /= 2;
		_video_end /= 2;
	} else if (frc.repeat) {
		_video_start *= 2;
		_video_end *= 2;
	}

	if (audio_sample_rate) {
		_audio_start = video_frames_to_audio_frames (_video_start, audio_sample_rate, frames_per_second);
		_audio_end = video_frames_to_audio_frames (_video_end, audio_sample_rate, frames_per_second);
	}

	/* XXX: this is a hack; this flag means that no trim is happening at the end of the film, and I'm
	   using that to prevent audio trim being rounded to video trim, which breaks the current set
	   of regression tests.  This could be removed if a) the regression tests are regenerated and b)
	   I can work out what DCP length should be.
	*/
	_no_trim = (_video_start == 0) && (_video_end == (video_length - video_trim_end));
}

void
Trimmer::process_video (shared_ptr<const Image> image, bool same, shared_ptr<Subtitle> sub)
{
	if (_no_trim || (_video_in >= _video_start && _video_in <= _video_end)) {
		Video (image, same, sub);
	}
	
	++_video_in;
}

void
Trimmer::process_audio (shared_ptr<const AudioBuffers> audio)
{
	if (_no_trim) {
		Audio (audio);
		return;
	}
	
	int64_t offset = _audio_start - _audio_in;
	if (offset > audio->frames()) {
		_audio_in += audio->frames ();
		return;
	}

	if (offset < 0) {
		offset = 0;
	}

	int64_t length = _audio_end - max (_audio_in, _audio_start);
	if (length < 0) {
		_audio_in += audio->frames ();
		return;
	}

	if (length > (audio->frames() - offset)) {
		length = audio->frames () - offset;
	}

	_audio_in += audio->frames ();
	
	if (offset != 0 || length != audio->frames ()) {
		shared_ptr<AudioBuffers> copy (new AudioBuffers (audio));
		copy->move (offset, 0, length);
		copy->set_frames (length);
		audio = copy;
	}
	
	Audio (audio);
}

