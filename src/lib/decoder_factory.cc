/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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

#include "ffmpeg_content.h"
#include "ffmpeg_decoder.h"
#include "dcp_content.h"
#include "dcp_decoder.h"
#include "image_content.h"
#include "image_decoder.h"
#include "text_subtitle_content.h"
#include "text_subtitle_decoder.h"
#include "dcp_subtitle_content.h"
#include "dcp_subtitle_decoder.h"
#include "video_mxf_content.h"
#include "video_mxf_decoder.h"
#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>

using std::list;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::make_shared;

shared_ptr<Decoder>
decoder_factory (shared_ptr<const Content> content, shared_ptr<Log> log, bool fast)
{
	shared_ptr<const FFmpegContent> fc = dynamic_pointer_cast<const FFmpegContent> (content);
	if (fc) {
		return make_shared<FFmpegDecoder> (fc, log, fast);
	}

	shared_ptr<const DCPContent> dc = dynamic_pointer_cast<const DCPContent> (content);
	if (dc) {
		return make_shared<DCPDecoder> (dc, log, fast);
	}

	shared_ptr<const ImageContent> ic = dynamic_pointer_cast<const ImageContent> (content);
	if (ic) {
		return make_shared<ImageDecoder> (ic, log);
	}

	shared_ptr<const TextSubtitleContent> rc = dynamic_pointer_cast<const TextSubtitleContent> (content);
	if (rc) {
		return make_shared<TextSubtitleDecoder> (rc);
	}

	shared_ptr<const DCPSubtitleContent> dsc = dynamic_pointer_cast<const DCPSubtitleContent> (content);
	if (dsc) {
		return make_shared<DCPSubtitleDecoder> (dsc);
	}

	shared_ptr<const VideoMXFContent> vmc = dynamic_pointer_cast<const VideoMXFContent> (content);
	if (vmc) {
		return make_shared<VideoMXFDecoder> (vmc, log);
	}

	return shared_ptr<Decoder> ();
}
