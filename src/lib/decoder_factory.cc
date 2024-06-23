/*
    Copyright (C) 2016-2020 Carl Hetherington <cth@carlh.net>

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


#include "atmos_mxf_content.h"
#include "atmos_mxf_decoder.h"
#include "dcp_content.h"
#include "dcp_decoder.h"
#include "dcp_subtitle_content.h"
#include "dcp_subtitle_decoder.h"
#include "ffmpeg_content.h"
#include "ffmpeg_decoder.h"
#include "image_content.h"
#include "image_decoder.h"
#include "string_text_file_content.h"
#include "string_text_file_decoder.h"
#include "timer.h"
#include "video_mxf_content.h"
#include "video_mxf_decoder.h"


using std::dynamic_pointer_cast;
using std::list;
using std::make_shared;
using std::shared_ptr;


template <class T>
shared_ptr<T>
maybe_cast (shared_ptr<Decoder> d)
{
	if (!d) {
		return {};
	}
	return dynamic_pointer_cast<T> (d);
}

/**
   @param tolerant true to proceed in the face of `survivable' errors, otherwise false.
   @param old_decoder A `used' decoder that has been previously made for this piece of content, or 0
*/
shared_ptr<Decoder>
decoder_factory (shared_ptr<const Film> film, shared_ptr<const Content> content, bool fast, bool tolerant, shared_ptr<Decoder> old_decoder)
{
	if (auto fc = dynamic_pointer_cast<const FFmpegContent>(content)) {
		return make_shared<FFmpegDecoder>(film, fc, fast);
	}

	if (auto dc = dynamic_pointer_cast<const DCPContent>(content)) {
		try {
			return make_shared<DCPDecoder>(film, dc, fast, tolerant, maybe_cast<DCPDecoder>(old_decoder));
		} catch (KDMError& e) {
			/* This will be found and reported to the user when the content is examined */
			return {};
		}
	}

	if (auto ic = dynamic_pointer_cast<const ImageContent>(content)) {
		return make_shared<ImageDecoder>(film, ic);
	}

	if (auto rc = dynamic_pointer_cast<const StringTextFileContent>(content)) {
		return make_shared<StringTextFileDecoder>(film, rc);
	}

	if (auto dsc = dynamic_pointer_cast<const DCPSubtitleContent>(content)) {
		return make_shared<DCPSubtitleDecoder>(film, dsc);
	}

	if (auto vmc = dynamic_pointer_cast<const VideoMXFContent>(content)) {
		return make_shared<VideoMXFDecoder>(film, vmc);
	}

	if (auto amc = dynamic_pointer_cast<const AtmosMXFContent>(content)) {
		return make_shared<AtmosMXFDecoder>(film, amc);
	}

	return {};
}
