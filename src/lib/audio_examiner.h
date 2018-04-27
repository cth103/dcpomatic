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

#ifndef DCPOMATIC_AUDIO_EXAMINER_H
#define DCPOMATIC_AUDIO_EXAMINER_H

/** @file  src/lib/audio_examiner.h
 *  @brief AudioExaminer class.
 */

#include "types.h"

/** @class AudioExaminer
 *  @brief Parent for classes which examine AudioContent for their pertinent details.
 */
class AudioExaminer
{
public:
	virtual ~AudioExaminer () {}

	virtual bool has_audio () const = 0;
	virtual int audio_channels () const = 0;
	virtual Frame audio_length () const = 0;
	virtual int audio_frame_rate () const = 0;
};

#endif
