/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


class VideoContent;
class ImageProxy;
class Image;
class Log;
class FrameIntervalChecker;


/** @class VideoDecoder
 *  @brief Parent for classes which decode video.
 */
class VideoDecoder : public DecoderPart
{
public:
	VideoDecoder (Decoder* parent, std::shared_ptr<const Content> c);

	friend struct video_decoder_fill_test1;
	friend struct video_decoder_fill_test2;
	friend struct ffmpeg_pts_offset_test;
	friend void ffmpeg_decoder_sequential_test_one (boost::filesystem::path file, float fps, int gaps, int video_length);

	boost::optional<dcpomatic::ContentTime> position (std::shared_ptr<const Film>) const {
		return _position;
	}

	void seek ();
	void emit (std::shared_ptr<const Film> film, std::shared_ptr<const ImageProxy>, Frame frame);

	boost::signals2::signal<void (ContentVideo)> Data;

private:
	std::shared_ptr<const Content> _content;
	/** Eyes of last thing to be emitted; only used for THREE_D_ALTERNATE */
	boost::optional<Eyes> _last_emitted_eyes;
	boost::optional<dcpomatic::ContentTime> _position;
	boost::scoped_ptr<FrameIntervalChecker> _frame_interval_checker;
};


#endif
