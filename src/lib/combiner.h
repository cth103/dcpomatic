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

/** @file src/lib/combiner.h
 *  @brief Class for combining two video streams.
 */

#include "processor.h"

/** @class Combiner
 *  @brief A class which can combine two video streams into one, with
 *  one image used for the left half of the screen and the other for
 *  the right.
 */
class Combiner : public TimedVideoProcessor
{
public:
	Combiner (boost::shared_ptr<Log> log);

	void process_video (boost::shared_ptr<const Image> i, bool, boost::shared_ptr<Subtitle> s, double);
	void process_video_b (boost::shared_ptr<const Image> i, bool, boost::shared_ptr<Subtitle> s, double);

private:
	/** The image that we are currently working on */
	boost::shared_ptr<Image> _image;
};
