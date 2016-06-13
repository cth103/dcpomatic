/*
    Copyright (C) 2012-2016 Carl Hetherington <cth@carlh.net>

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
#include <boost/signals2.hpp>
#include <boost/shared_ptr.hpp>

class VideoContent;
class ImageProxy;
class Image;
class Log;

/** @class VideoDecoder
 *  @brief Parent for classes which decode video.
 */
class VideoDecoder
{
public:
	VideoDecoder (Decoder* parent, boost::shared_ptr<const Content> c, boost::shared_ptr<Log> log);

	std::list<ContentVideo> get (Frame frame, bool accurate);

	void set_ignore ();
	bool ignore () const {
		return _ignore;
	}

#ifdef DCPOMATIC_DEBUG
	int test_gaps;
#endif

	friend struct video_decoder_fill_test1;
	friend struct video_decoder_fill_test2;
	friend struct ffmpeg_pts_offset_test;
	friend void ffmpeg_decoder_sequential_test_one (boost::filesystem::path file, float fps, int gaps, int video_length);

	void seek (ContentTime time, bool accurate);
	void give (boost::shared_ptr<const ImageProxy>, Frame frame);

private:

	std::list<ContentVideo> decoded (Frame frame);
	void fill_one_eye (Frame from, Frame to, Eyes);
	void fill_both_eyes (Frame from_frame, Eyes from_eyes, Frame to_frame, Eyes to_eyes);

	Decoder* _parent;
	boost::shared_ptr<const Content> _content;
	boost::shared_ptr<Log> _log;
	std::list<ContentVideo> _decoded;
	boost::shared_ptr<Image> _black_image;
	boost::optional<ContentTime> _last_seek_time;
	bool _last_seek_accurate;
	/** true if this decoder should ignore all video; i.e. never produce any */
	bool _ignore;
	/** if set, this is a frame for which we got no data because the Decoder said
	 *  it has no more to give.
	 */
	boost::optional<Frame> _no_data_frame;
};

#endif
