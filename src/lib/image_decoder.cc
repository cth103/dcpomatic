/*
    Copyright (C) 2012-2013 Carl Hetherington <cth@carlh.net>

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

#include <iostream>
#include <boost/filesystem.hpp>
#include <Magick++.h>
#include "image_content.h"
#include "image_decoder.h"
#include "image.h"
#include "image_proxy.h"
#include "film.h"
#include "exceptions.h"

#include "i18n.h"

using std::cout;
using boost::shared_ptr;
using libdcp::Size;

ImageDecoder::ImageDecoder (shared_ptr<const Film> f, shared_ptr<const ImageContent> c)
	: Decoder (f)
	, VideoDecoder (f, c)
	, _image_content (c)
{

}

void
ImageDecoder::pass ()
{
	if (_video_position >= _image_content->video_length ()) {
		return;
	}

	if (_image && _image_content->still ()) {
		video (_image, true, _video_position);
		return;
	}

	_image.reset (new MagickImageProxy (_image_content->path (_image_content->still() ? 0 : _video_position)));
	video (_image, false, _video_position);
}

void
ImageDecoder::seek (VideoContent::Frame frame, bool)
{
	_video_position = frame;
}

bool
ImageDecoder::done () const
{
	return _video_position >= _image_content->video_length ();
}
