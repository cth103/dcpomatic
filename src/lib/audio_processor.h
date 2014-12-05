/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_AUDIO_PROCESSOR_H
#define DCPOMATIC_AUDIO_PROCESSOR_H

#include "channel_count.h"
#include <boost/shared_ptr.hpp>
#include <list>
#include <string>

class AudioBuffers;

class AudioProcessor
{
public:
	virtual ~AudioProcessor () {}

	virtual std::string name () const = 0;
	virtual std::string id () const = 0;
	virtual ChannelCount in_channels () const = 0;
	virtual int out_channels (int) const = 0;
	virtual boost::shared_ptr<AudioProcessor> clone (int sampling_rate) const = 0;
	virtual boost::shared_ptr<AudioBuffers> run (boost::shared_ptr<const AudioBuffers>) = 0;
	virtual void flush () {}

	static std::list<AudioProcessor const *> all ();
	static void setup_audio_processors ();
	static AudioProcessor const * from_id (std::string);

private:
	static std::list<AudioProcessor const *> _all;
};

#endif
