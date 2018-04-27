/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

/** @file  src/lib/video_examiner.h
 *  @brief VideoExaminer class.
 */

#include "types.h"
#include "video_content.h"
#include <dcp/types.h>

/** @class VideoExaminer
 *  @brief Parent for classes which examine video sources and obtain information about them.
 */
class VideoExaminer
{
public:
	virtual ~VideoExaminer () {}

	virtual bool has_video () const = 0;

	/** @return video frame rate (if known); must not be called if has_video() == false */
	virtual boost::optional<double> video_frame_rate () const = 0;
	/** @return video size; must not be called if has_video() == false */
	virtual dcp::Size video_size () const = 0;
	/** @return video length in frames; must not be called if has_video() == false */
	virtual Frame video_length () const = 0;
	/** @return video sample aspect ratio (if known); must not be called if has_video() == false */
	virtual boost::optional<double> sample_aspect_ratio () const {
		return boost::optional<double> ();
	}
	/** @return true if this video is in YUV; must not be called if has_video() == false */
	virtual bool yuv () const = 0;
};
