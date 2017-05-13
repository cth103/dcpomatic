/*
    Copyright (C) 2013-2017 Carl Hetherington <cth@carlh.net>

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

/** @file  src/audio_merger.cc
 *  @brief AudioMerger class.
 */

#include "audio_merger.h"
#include "dcpomatic_time.h"
#include <iostream>

using std::pair;
using std::min;
using std::max;
using std::list;
using std::cout;
using std::make_pair;
using boost::shared_ptr;
using boost::optional;

AudioMerger::AudioMerger (int frame_rate)
	: _frame_rate (frame_rate)
{

}

Frame
AudioMerger::frames (DCPTime t) const
{
	return t.frames_floor (_frame_rate);
}

/** Pull audio up to a given time; after this call, no more data can be pushed
 *  before the specified time.
 *  @param time Time to pull up to.
 *  @return Blocks of merged audio up to `time'.
 */
list<pair<shared_ptr<AudioBuffers>, DCPTime> >
AudioMerger::pull (DCPTime time)
{
	list<pair<shared_ptr<AudioBuffers>, DCPTime> > out;

	DCPTimePeriod period (_last_pull, time);
	_buffers.sort (AudioMerger::BufferComparator());

	list<Buffer> new_buffers;

	BOOST_FOREACH (Buffer i, _buffers) {
		if (i.period().to <= time) {
			/* Completely within the pull period */
			DCPOMATIC_ASSERT (i.audio->frames() > 0);
			out.push_back (make_pair (i.audio, i.time));
		} else if (i.time < time) {
			/* Overlaps the end of the pull period */
			shared_ptr<AudioBuffers> audio (new AudioBuffers (i.audio->channels(), frames(DCPTime(time - i.time))));
			/* Though time > i.time, audio->frames() could be 0 if the difference in time is less than one frame */
			if (audio->frames() > 0) {
				audio->copy_from (i.audio.get(), audio->frames(), 0, 0);
				out.push_back (make_pair (audio, i.time));
				i.audio->trim_start (audio->frames ());
				i.time += DCPTime::from_frames(audio->frames(), _frame_rate);
				DCPOMATIC_ASSERT (i.audio->frames() > 0);
				new_buffers.push_back (i);
			}
		} else {
			/* Not involved */
			DCPOMATIC_ASSERT (i.audio->frames() > 0);
			new_buffers.push_back (i);
		}
	}

	_buffers = new_buffers;

	for (list<pair<shared_ptr<AudioBuffers>, DCPTime> >::const_iterator i = out.begin(); i != out.end(); ++i) {
		DCPOMATIC_ASSERT (i->first->frames() > 0);
	}

	return out;
}

/** Push some data into the merger at a given time */
void
AudioMerger::push (boost::shared_ptr<const AudioBuffers> audio, DCPTime time)
{
	DCPOMATIC_ASSERT (time >= _last_pull);
	DCPOMATIC_ASSERT (audio->frames() > 0);

	DCPTimePeriod period (time, time + DCPTime::from_frames (audio->frames(), _frame_rate));

	/* Mix any overlapping parts of this new block with existing ones */
	BOOST_FOREACH (Buffer i, _buffers) {
		optional<DCPTimePeriod> overlap = i.period().overlap (period);
		if (overlap) {
			int32_t const offset = frames(DCPTime(overlap->from - i.time));
			int32_t const frames_to_mix = frames(overlap->duration());
			if (i.time < time) {
				i.audio->accumulate_frames(audio.get(), frames_to_mix, 0, offset);
			} else {
				i.audio->accumulate_frames(audio.get(), frames_to_mix, offset, 0);
			}
		}
	}

	list<DCPTimePeriod> periods;
	BOOST_FOREACH (Buffer i, _buffers) {
		periods.push_back (i.period ());
	}

	/* Add the non-overlapping parts */
	BOOST_FOREACH (DCPTimePeriod i, subtract (period, periods)) {
		list<Buffer>::iterator before = _buffers.end();
		list<Buffer>::iterator after = _buffers.end();
		for (list<Buffer>::iterator j = _buffers.begin(); j != _buffers.end(); ++j) {
			if (j->period().to == i.from) {
				before = j;
			}
			if (j->period().from == i.to) {
				after = j;
			}
		}

		/* Get the part of audio that we want to use */
		shared_ptr<AudioBuffers> part (new AudioBuffers (audio->channels(), frames(i.to) - frames(i.from)));
		part->copy_from (audio.get(), part->frames(), frames(DCPTime(i.from - time)), 0);

		if (before == _buffers.end() && after == _buffers.end()) {
			/* New buffer */
			DCPOMATIC_ASSERT (part->frames() > 0);
			_buffers.push_back (Buffer (part, time, _frame_rate));
		} else if (before != _buffers.end() && after == _buffers.end()) {
			/* We have an existing buffer before this one; append new data to it */
			before->audio->append (part);
		} else if (before ==_buffers.end() && after != _buffers.end()) {
			/* We have an existing buffer after this one; append it to the new data and replace */
			part->append (after->audio);
			after->audio = part;
			after->time = time;
		} else {
			/* We have existing buffers both before and after; coalesce them all */
			before->audio->append (part);
			before->audio->append (after->audio);
			_buffers.erase (after);
		}
	}
}

void
AudioMerger::clear ()
{
	_buffers.clear ();
	_last_pull = DCPTime ();
}
