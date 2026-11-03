/*
    Copyright (C) 2026 Carl Hetherington <cth@carlh.net>

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


/** @file  src/lib/dcdm_quality.h
 *  @brief Check that the content of a film is good enough for a DCDM.
 */


#ifndef DCPOMATIC_DCDM_QUALITY_H
#define DCPOMATIC_DCDM_QUALITY_H


#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
extern "C" {
#include <libavcodec/avcodec.h>
}
LIBDCP_ENABLE_WARNINGS
#include <boost/filesystem.hpp>
#include <memory>
#include <string>


class Film;


/** Result of checking whether a film's sources are good enough for a DCDM export */
struct DCDMQuality
{
	bool ok;
	/** Empty if ok, otherwise names the first failing content and says why */
	std::string reason;
};


/** @return true if a video stream with this codec, profile and pixel format is
 *  good enough to export as a DCDM: an intraframe or lossless codec, 10 bits
 *  per component or more, chroma 4:2:2 or better.
 *  @param profile Codec profile as set by FFmpeg (e.g. ProRes profile 3 is HQ).
 */
bool dcdm_ffmpeg_ok(AVCodecID codec, int profile, AVPixelFormat pix_fmt);

/** @return true if still / sequence image files like @param first_file (judged by extension)
 *  with this decoded pixel format are good enough to export as a DCDM:
 *  TIFF, DPX or EXR at 10 bits per component or more.
 */
bool dcdm_image_ok(boost::filesystem::path first_file, AVPixelFormat pix_fmt);

/** Check every video content in a film.  All must be good enough.  Audio-only and
 *  text content are ignored; DCP content never qualifies (re-encoding a DCDM back to
 *  a DCP would add a JPEG 2000 generation).  Content examined before codec and pixel
 *  format were recorded does not qualify until it is re-examined.
 */
DCDMQuality dcdm_source_quality(std::shared_ptr<const Film> film);


#endif
