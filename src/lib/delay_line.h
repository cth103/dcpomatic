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

#include <boost/shared_ptr.hpp>
#include "processor.h"

class AudioBuffers;

/** A delay line for audio */
class DelayLine : public Processor, public TimedAudioSink, public TimedAudioSource
{
public:
	DelayLine (Log* log, int channels, int frames);
	
	void process_audio (boost::shared_ptr<AudioBuffers>, double);

private:
	boost::shared_ptr<AudioBuffers> _buffers;
	int _negative_delay_remaining; ///< number of frames of negative delay that remain to emit
	int _frames;
};
