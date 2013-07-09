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

#include "combiner.h"
#include "image.h"

using boost::shared_ptr;

Combiner::Combiner (shared_ptr<Log> log)
	: TimedVideoProcessor (log)
{

}

/** Process video for the left half of the frame.
 *  Subtitle parameter will be ignored.
 *  @param image Frame image.
 */
void
Combiner::process_video (shared_ptr<const Image> image, bool, shared_ptr<Subtitle>, double)
{
	_image.reset (new SimpleImage (image));
}

/** Process video for the right half of the frame.
 *  @param image Frame image.
 *  @param sub Subtitle (which will be put onto the whole frame)
 */
void
Combiner::process_video_b (shared_ptr<const Image> image, bool, shared_ptr<Subtitle> sub, double t)
{
	if (!_image) {
		/* It's possible for filters in the A-side to mean that we get a B frame
		   before any A; just skip the B frame in that case.  This at least prevents
		   a crash, but may not be right.
		*/
		return;
	}
	
	/* Copy the right half of this image into our _image */
	/* XXX: this should probably be in the Image class */
	for (int i = 0; i < image->components(); ++i) {
		int const line_size = image->line_size()[i];
		int const half_line_size = line_size / 2;

		uint8_t* p = _image->data()[i];
		uint8_t* q = image->data()[i];
			
		for (int j = 0; j < image->lines (i); ++j) {
			memcpy (p + half_line_size, q + half_line_size, half_line_size);
			p += _image->stride()[i];
			q += image->stride()[i];
		}
	}

	Video (_image, false, sub, t);
	_image.reset ();
}
