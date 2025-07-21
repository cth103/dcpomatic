/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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


#include "mpeg2_encoder.h"
#include "writer.h"
#include <dcp/ffmpeg_image.h>
extern "C" {
#include <libavutil/pixfmt.h>
}


using std::shared_ptr;


MPEG2Encoder::MPEG2Encoder(shared_ptr<const Film> film, Writer& writer)
	: VideoEncoder(film, writer)
	, _transcoder(film->frame_size(), film->video_frame_rate(), film->video_bit_rate(VideoEncoding::MPEG2))
{

}


void
MPEG2Encoder::encode(shared_ptr<PlayerVideo> pv, dcpomatic::DCPTime time)
{
	VideoEncoder::encode(pv, time);

	auto image = pv->image(force(AV_PIX_FMT_YUV420P), VideoRange::VIDEO, false);

	dcp::FFmpegImage ffmpeg_image(time.get() * _film->video_frame_rate() / dcpomatic::DCPTime::HZ);

	DCPOMATIC_ASSERT(image->size() == ffmpeg_image.size());

	auto height = image->size().height;

	for (int y = 0; y < height; ++y) {
		memcpy(ffmpeg_image.y() + ffmpeg_image.y_stride() * y, image->data()[0] + image->stride()[0] * y, ffmpeg_image.y_stride());
	}

	for (int y = 0; y < height / 2; ++y) {
		memcpy(ffmpeg_image.u() + ffmpeg_image.u_stride() * y, image->data()[1] + image->stride()[1] * y, ffmpeg_image.u_stride());
		memcpy(ffmpeg_image.v() + ffmpeg_image.v_stride() * y, image->data()[2] + image->stride()[2] * y, ffmpeg_image.v_stride());
	}

	if (auto compressed = _transcoder.compress_frame(std::move(ffmpeg_image))) {
		_writer.write(compressed->first, compressed->second);
	}
}


void
MPEG2Encoder::end()
{
	if (auto compressed = _transcoder.flush()) {
		_writer.write(compressed->first, compressed->second);
	}
}

