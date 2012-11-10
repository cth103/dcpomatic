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

/** @file src/imagemagick_encoder.h
 *  @brief An encoder that writes image files using ImageMagick (and does nothing with audio).
 */

#include <string>
#include <sndfile.h>
#include "encoder.h"

class FilmState;
class Log;

/** @class ImageMagickEncoder
 *  @brief An encoder that writes image files using ImageMagick files (and does nothing with audio).
 */
class ImageMagickEncoder : public Encoder
{
public:
	ImageMagickEncoder (boost::shared_ptr<const Film> f, boost::shared_ptr<const Options> o);

private:	
	void do_process_video (boost::shared_ptr<const Image>, SourceFrame, boost::shared_ptr<Subtitle>);
	void do_process_audio (boost::shared_ptr<const AudioBuffers>) {}
};
