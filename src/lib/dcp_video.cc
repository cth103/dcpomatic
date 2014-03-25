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

#include "dcp_video.h"
#include "image.h"

using boost::shared_ptr;

/** From ContentVideo:
 *  @param in Image.
 *  @param eyes Eye(s) that the Image is for.
 *
 *  From Content:
 *  @param crop Crop to apply.
 *  @param inter_size
 *  @param out_size
 *  @param scaler Scaler to use.
 *  @param conversion Colour conversion to use.
 *
 *  @param time DCP time.
 */
DCPVideo::DCPVideo (
	shared_ptr<const Image> in,
	Eyes eyes,
	Crop crop,
	dcp::Size inter_size,
	dcp::Size out_size,
	Scaler const * scaler,
	ColourConversion conversion,
	DCPTime time
	)
	: _in (in)
	, _eyes (eyes)
	, _crop (crop)
	, _inter_size (inter_size)
	, _out_size (out_size)
	, _scaler (scaler)
	, _conversion (conversion)
	, _time (time)
{

}

void
DCPVideo::set_subtitle (PositionImage s)
{
	_subtitle = s;
}

shared_ptr<Image>
DCPVideo::image (AVPixelFormat format, bool aligned) const
{
	shared_ptr<Image> out = _in->crop_scale_window (_crop, _inter_size, _out_size, _scaler, format, aligned);
	
	Position<int> const container_offset ((_out_size.width - _inter_size.width) / 2, (_out_size.height - _inter_size.width) / 2);

	if (_subtitle.image) {
		out->alpha_blend (_subtitle.image, _subtitle.position);
	}

	return out;
}

