/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "player_video_frame.h"
#include "image.h"

using boost::shared_ptr;

PlayerVideoFrame::PlayerVideoFrame (
	shared_ptr<const Image> in,
	Crop crop,
	libdcp::Size inter_size,
	libdcp::Size out_size,
	Scaler const * scaler
	)
	: _in (in)
	, _crop (crop)
	, _inter_size (inter_size)
	, _out_size (out_size)
	, _scaler (scaler)
{

}

void
PlayerVideoFrame::set_subtitle (shared_ptr<const Image> image, Position<int> pos)
{
	_subtitle_image = image;
	_subtitle_position = pos;
}

shared_ptr<Image>
PlayerVideoFrame::image ()
{
	shared_ptr<Image> out = _in->crop_scale_window (_crop, _inter_size, _out_size, _scaler, PIX_FMT_RGB24, false);

	Position<int> const container_offset ((_out_size.width - _inter_size.width) / 2, (_out_size.height - _inter_size.width) / 2);

	if (_subtitle_image) {
		out->alpha_blend (_subtitle_image, _subtitle_position);
	}

	return out;
}
