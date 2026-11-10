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


#include "content.h"
#include "dcdm_quality.h"
#include "dcp_content.h"
#include "ffmpeg_content.h"
#include "film.h"
#include "image_content.h"
#include "i18n.h"
#include "video_content.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
extern "C" {
#include <libavutil/pixdesc.h>
}
LIBDCP_ENABLE_WARNINGS
#include <fmt/format.h>
#include <boost/algorithm/string.hpp>


using std::shared_ptr;
using std::string;


/* Profile numbers as used by FFmpeg's ProRes and DNx decoders.  These are written
 * out here rather than using FF_PROFILE_* / AV_PROFILE_* because the macro names
 * differ between FFmpeg versions.
 */
static int constexpr PRORES_HQ = 3;
static int constexpr PRORES_4444 = 4;
static int constexpr PRORES_XQ = 5;
static int constexpr DNXHR_HQX = 4;
static int constexpr DNXHR_444 = 5;


static
bool
depth_and_chroma_ok(AVPixelFormat pix_fmt)
{
	auto const desc = av_pix_fmt_desc_get(pix_fmt);
	if (!desc) {
		return false;
	}

	if (desc->comp[0].depth < 10) {
		return false;
	}

	/* 4:4:4 or 4:2:2 (chroma halved horizontally only) */
	return desc->log2_chroma_h == 0 && desc->log2_chroma_w <= 1;
}


bool
dcdm_ffmpeg_ok(AVCodecID codec, int profile, AVPixelFormat pix_fmt)
{
	if (!depth_and_chroma_ok(pix_fmt)) {
		return false;
	}

	switch (codec) {
	case AV_CODEC_ID_PRORES:
		return profile == PRORES_HQ || profile == PRORES_4444 || profile == PRORES_XQ;
	case AV_CODEC_ID_DNXHD:
		return profile == DNXHR_HQX || profile == DNXHR_444;
	case AV_CODEC_ID_FFV1:
	case AV_CODEC_ID_V210:
	case AV_CODEC_ID_RAWVIDEO:
		return true;
	default:
		return false;
	}
}


bool
dcdm_image_ok(boost::filesystem::path first_file, AVPixelFormat pix_fmt)
{
	auto extension = first_file.extension().string();
	boost::algorithm::to_lower(extension);
	if (extension != ".tif" && extension != ".tiff" && extension != ".dpx" && extension != ".exr") {
		return false;
	}

	auto const desc = av_pix_fmt_desc_get(pix_fmt);
	return desc && desc->comp[0].depth >= 10;
}


static
DCDMQuality
fail(shared_ptr<const Content> content, string why)
{
	auto const name = content->number_of_paths() > 0 ? content->path(0).filename().string() : string("(unknown content)");
	return { false, fmt::format("{}: {}", name, why) };
}


DCDMQuality
dcdm_source_quality(shared_ptr<const Film> film)
{
	bool any_video = false;

	for (auto content: film->content()) {
		/* Video that is switched off does not go into the DCP, so it does not matter here */
		if (!content->video || !content->video->use()) {
			continue;
		}
		any_video = true;

		if (auto ffmpeg = std::dynamic_pointer_cast<const FFmpegContent>(content)) {
			auto const codec = ffmpeg->video_codec_id();
			auto const profile = ffmpeg->video_profile();
			auto const pix_fmt = ffmpeg->video_pixel_format();
			if (!codec || !profile || !pix_fmt) {
				return fail(content, _("codec and pixel format are not recorded; examine this content again"));
			}
			if (!dcdm_ffmpeg_ok(*codec, *profile, *pix_fmt)) {
				return fail(content, _("video quality is too low for a DCDM (needs an intraframe or lossless codec such as ProRes HQ/4444/4444 XQ, DNxHR HQX/444, FFV1, v210 or uncompressed, at 10 bits or more and 4:2:2 or better)"));
			}
		} else if (auto image = std::dynamic_pointer_cast<const ImageContent>(content)) {
			auto const pix_fmt = image->pixel_format();
			if (!pix_fmt) {
				return fail(content, _("pixel format is not recorded; examine this content again"));
			}
			if (!dcdm_image_ok(content->path(0), *pix_fmt)) {
				return fail(content, _("images must be TIFF, DPX or EXR at 10 bits per component or more"));
			}
		} else if (std::dynamic_pointer_cast<const DCPContent>(content)) {
			return fail(content, _("DCP content is not accepted: re-encoding a DCDM to a DCP would add a JPEG 2000 generation"));
		} else {
			return fail(content, _("this type of video content is not supported for a DCDM"));
		}
	}

	if (!any_video) {
		return { false, _("the film has no video content") };
	}

	return { true, string() };
}
