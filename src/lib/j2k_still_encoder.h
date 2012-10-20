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

/** @file  src/j2k_still_encoder.h
 *  @brief An encoder which writes JPEG2000 files for a single still source image.
 */

#include <list>
#include <vector>
#include "encoder.h"

class Image;
class Log;

/** @class J2KStillEncoder
 *  @brief An encoder which writes repeated JPEG2000 files from a single decoded input.
 */
class J2KStillEncoder : public Encoder
{
public:
	J2KStillEncoder (boost::shared_ptr<const FilmState>, boost::shared_ptr<const Options>, Log *);

	void process_begin (int64_t audio_channel_layout, AVSampleFormat audio_sample_format) {}
	void process_video (boost::shared_ptr<Image>, int, boost::shared_ptr<Subtitle>);
	void process_audio (boost::shared_ptr<const AudioBuffers>) {}
	void process_end () {}
};
