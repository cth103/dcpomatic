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
#include "string_text_file_content.h"
#include "string_text_file_decoder.h"
#include "dcp_subtitle_content.h"
#include "dcp_subtitle_decoder.h"
#include "video_mxf_content.h"
#include "video_mxf_decoder.h"
#include <boost/foreach.hpp>

using std::list;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

shared_ptr<Decoder>
decoder_factory (shared_ptr<const Film> film, shared_ptr<const Content> content, bool fast)
{
	shared_ptr<const FFmpegContent> fc = dynamic_pointer_cast<const FFmpegContent> (content);
	if (fc) {
		return shared_ptr<Decoder> (new FFmpegDecoder(film, fc, fast));
	}

	shared_ptr<const DCPContent> dc = dynamic_pointer_cast<const DCPContent> (content);
	if (dc) {
		try {
			return shared_ptr<Decoder> (new DCPDecoder(film, dc, fast));
		} catch (KDMError& e) {
			/* This will be found and reported to the user when the content is examined */
			return shared_ptr<Decoder>();
		}
	}

	shared_ptr<const ImageContent> ic = dynamic_pointer_cast<const ImageContent> (content);
	if (ic) {
		return shared_ptr<Decoder> (new ImageDecoder(film, ic));
	}

	shared_ptr<const StringTextFileContent> rc = dynamic_pointer_cast<const StringTextFileContent> (content);
	if (rc) {
		return shared_ptr<Decoder> (new StringTextFileDecoder(film, rc));
	}

	shared_ptr<const DCPSubtitleContent> dsc = dynamic_pointer_cast<const DCPSubtitleContent> (content);
	if (dsc) {
		return shared_ptr<Decoder> (new DCPSubtitleDecoder(film, dsc));
	}

	shared_ptr<const VideoMXFContent> vmc = dynamic_pointer_cast<const VideoMXFContent> (content);
	if (vmc) {
		return shared_ptr<Decoder> (new VideoMXFDecoder(film, vmc));
	}

	return shared_ptr<Decoder> ();
}
