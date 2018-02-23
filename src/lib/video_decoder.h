/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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

/** @file  src/lib/video_decoder.h
 *  @brief VideoDecoder class.
 */

#ifndef DCPOMATIC_VIDEO_DECODER_H
#define DCPOMATIC_VIDEO_DECODER_H

#include "decoder.h"
#include "video_content.h"
#include "util.h"
#include "content_video.h"
#include "decoder_part.h"
#include <boost/signals2.hpp>
#include <boost/shared_ptr.hpp>

class VideoContent;
class ImageProxy;
class Image;
class Log;

/** @class VideoDecoder
 *  @brief Parent for classes which decode video.
 */
class VideoDecoder : public DecoderPart
{
public:
	VideoDecoder (Decoder* parent, boost::shared_ptr<const Content> c, boost::shared_ptr<Log> log);

	friend struct video_decoder_fill_test1;
	friend struct video_decoder_fill_test2;
	friend struct ffmpeg_pts_offset_test;
	friend void ffmpeg_decoder_sequential_test_one (boost::filesystem::path file, float fps, int gaps, int video_length);

	ContentTime position () const {
		return _position;
	}

	void seek ();

	void emit (boost::shared_ptr<const ImageProxy>, Frame frame);

	/** @return true if the emitted data was accepted, false if not */
	boost::signals2::signal<void (ContentVideo)> Data;

private:
	/** Time of last thing to be emitted */
	boost::shared_ptr<const Content> _content;
	boost::optional<Frame> _last_emitted;
	ContentTime _position;
};

#endif
