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

/** @file  src/audio_source.h
 *  @brief Parent class for classes which emit audio data.
 */

#ifndef DCPOMATIC_AUDIO_SOURCE_H
#define DCPOMATIC_AUDIO_SOURCE_H

#include <boost/signals2.hpp>

class AudioBuffers;
class AudioSink;

/** A class that emits audio data */
class AudioSource
{
public:
	/** Emitted when some audio data is ready */
	boost::signals2::signal<void (boost::shared_ptr<AudioBuffers>)> Audio;

	void connect_audio (boost::shared_ptr<AudioSink>);
};

#endif
