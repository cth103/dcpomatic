/*
    Copyright (C) 2016-2017 Carl Hetherington <cth@carlh.net>

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

#include "video_mxf_decoder.h"
#include "video_decoder.h"
#include "video_mxf_content.h"
#include "j2k_image_proxy.h"
#include <dcp/mono_picture_asset.h>
#include <dcp/mono_picture_asset_reader.h>
#include <dcp/stereo_picture_asset.h>
#include <dcp/stereo_picture_asset_reader.h>
#include <dcp/exceptions.h>

using boost::shared_ptr;
using boost::optional;

VideoMXFDecoder::VideoMXFDecoder (shared_ptr<const VideoMXFContent> content, shared_ptr<Log> log)
	: _content (content)
{
	video.reset (new VideoDecoder (this, content, log));

	shared_ptr<dcp::MonoPictureAsset> mono;
	try {
		mono.reset (new dcp::MonoPictureAsset (_content->path(0)));
	} catch (dcp::MXFFileError& e) {
		/* maybe it's stereo */
	} catch (dcp::DCPReadError& e) {
		/* maybe it's stereo */
	}

	shared_ptr<dcp::StereoPictureAsset> stereo;
	try {
		stereo.reset (new dcp::StereoPictureAsset (_content->path(0)));
	} catch (dcp::MXFFileError& e) {
		if (!mono) {
			throw;
		}
	} catch (dcp::DCPReadError& e) {
		if (!mono) {
			throw;
		}
	}

	if (mono) {
		_mono_reader = mono->start_read ();
		_size = mono->size ();
	} else {
		_stereo_reader = stereo->start_read ();
		_size = stereo->size ();
	}
}

bool
VideoMXFDecoder::pass ()
{
	double const vfr = _content->active_video_frame_rate ();
	int64_t const frame = _next.frames_round (vfr);

	if (frame >= _content->video->length()) {
		return true;
	}

	if (_mono_reader) {
		video->emit (
			shared_ptr<ImageProxy> (
				new J2KImageProxy (_mono_reader->get_frame(frame), _size, AV_PIX_FMT_XYZ12LE, optional<int>())
				),
			frame
			);
	} else {
		video->emit (
			shared_ptr<ImageProxy> (
				new J2KImageProxy (_stereo_reader->get_frame(frame), _size, dcp::EYE_LEFT, AV_PIX_FMT_XYZ12LE, optional<int>())
				),
			frame
			);
		video->emit (
			shared_ptr<ImageProxy> (
				new J2KImageProxy (_stereo_reader->get_frame(frame), _size, dcp::EYE_RIGHT, AV_PIX_FMT_XYZ12LE, optional<int>())
				),
			frame
			);
	}

	_next += ContentTime::from_frames (1, vfr);
	return false;
}

void
VideoMXFDecoder::seek (ContentTime t, bool accurate)
{
	Decoder::seek (t, accurate);
	_next = t;
}
