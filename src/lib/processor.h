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

/** @file  src/processor.h
 *  @brief Parent class for classes which accept and then emit video or audio data.
 */

#ifndef DVDOMATIC_PROCESSOR_H
#define DVDOMATIC_PROCESSOR_H

#include "video_source.h"
#include "video_sink.h"
#include "audio_source.h"
#include "audio_sink.h"

class Log;

/** @class Processor
 *  @brief Base class for processors.
 */
class Processor
{
public:
	/** Construct a Processor.
	 *  @param log Log to use.
	 */
	Processor (Log* log)
		: _log (log)
	{}

	/** Will be called at the end of a processing run */
	virtual void process_end () {}

protected:
	Log* _log; ///< log to write to
};

/** @class AudioVideoProcessor
 *  @brief A processor which handles both video and audio data.
 */
class AudioVideoProcessor : public Processor, public VideoSource, public VideoSink, public AudioSource, public AudioSink
{
public:
	/** Construct an AudioVideoProcessor.
	 *  @param log Log to write to.
	 */
	AudioVideoProcessor (Log* log)
		: Processor (log)
	{}
};

/** @class AudioProcessor
 *  @brief A processor which handles just audio data.
 */
class AudioProcessor : public Processor, public AudioSource, public AudioSink
{
public:
	/** Construct an AudioProcessor.
	 *  @param log Log to write to.
	 */
	AudioProcessor (Log* log)
		: Processor (log)
	{}
};

/** @class VideoProcessor
 *  @brief A processor which handles just video data.
 */
class VideoProcessor : public Processor, public VideoSource, public VideoSink
{
public:
	/** Construct an VideoProcessor.
	 *  @param log Log to write to.
	 */
	VideoProcessor (Log* log)
		: Processor (log)
	{}
};

#endif
