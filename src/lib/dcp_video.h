/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>
    Taken from code Copyright (C) 2010-2011 Terrence Meiczinger

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

#include "util.h"
#include <dcp/picture_mxf_writer.h>

/** @file  src/dcp_video_frame.h
 *  @brief A single frame of video destined for a DCP.
 */

class Film;
class ServerDescription;
class Scaler;
class Image;
class Log;
class Subtitle;
class PlayerVideo;
class EncodedData;

/** @class DCPVideo
 *  @brief A single frame of video destined for a DCP.
 *
 *  Given an Image and some settings, this class knows how to encode
 *  the image to J2K either on the local host or on a remote server.
 *
 *  Objects of this class are used for the queue that we keep
 *  of images that require encoding.
 */
class DCPVideo : public boost::noncopyable
{
public:
	DCPVideo (boost::shared_ptr<const PlayerVideo>, int, int, int, Resolution, bool b, boost::shared_ptr<Log>);
	DCPVideo (boost::shared_ptr<const PlayerVideo>, cxml::ConstNodePtr, boost::shared_ptr<Log>);

	boost::shared_ptr<EncodedData> encode_locally ();
	boost::shared_ptr<EncodedData> encode_remotely (ServerDescription);

	int index () const {
		return _index;
	}

	Eyes eyes () const;

	bool same (boost::shared_ptr<const DCPVideo> other) const;
	
private:

	void add_metadata (xmlpp::Element *) const;
	
	boost::shared_ptr<const PlayerVideo> _frame;
	int _index;			 ///< frame index within the DCP's intrinsic duration
	int _frames_per_second;		 ///< Frames per second that we will use for the DCP
	int _j2k_bandwidth;		 ///< J2K bandwidth to use
	Resolution _resolution;          ///< Resolution (2K or 4K)
	bool _burn_subtitles;            ///< true to burn subtitles into the image

	boost::shared_ptr<Log> _log; ///< log
};
