/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef DCPOMATIC_CONTENT_AUDIO_H
#define DCPOMATIC_CONTENT_AUDIO_H

/** @file  src/lib/content_audio.h
 *  @brief ContentAudio class.
 */

#include "audio_buffers.h"
#include "types.h"

/** @class ContentAudio
 *  @brief A block of audio from a piece of content, with a timestamp as a frame within that content.
 */
class ContentAudio
{
public:
	ContentAudio ()
		: audio (new AudioBuffers (0, 0))
		, frame (0)
	{}

	ContentAudio (boost::shared_ptr<const AudioBuffers> a, Frame f)
		: audio (a)
		, frame (f)
	{}

	boost::shared_ptr<const AudioBuffers> audio;
	Frame frame;
};

#endif
