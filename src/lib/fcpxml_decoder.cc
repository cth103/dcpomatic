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


#include "fcpxml_content.h"
#include "fcpxml_decoder.h"
#include "ffmpeg_image_proxy.h"
#include "guess_crop.h"
#include "image.h"
#include "rect.h"
#include "text_decoder.h"
#include <dcp/array_data.h>



using std::make_shared;
using std::shared_ptr;
using std::weak_ptr;


FCPXMLDecoder::FCPXMLDecoder(weak_ptr<const Film> film, shared_ptr<const FCPXMLContent> content)
	: Decoder(film)
	, _fcpxml_content(content)
	, _sequence(dcpomatic::fcpxml::load(content->path(0)))
{
	text.push_back(make_shared<TextDecoder>(this, content->only_text()));
	update_position();
}


bool
FCPXMLDecoder::pass()
{
	if (_next >= static_cast<int>(_sequence.video.size())) {
		return true;
	}

	auto const png_data = dcp::ArrayData(_sequence.parent / _sequence.video[_next].source);
	auto const full_image = FFmpegImageProxy(png_data).image(Image::Alignment::PADDED).image;
	auto const crop = guess_crop_by_alpha(full_image);
	auto const cropped_image = full_image->crop(crop);

	auto rectangle = dcpomatic::Rect<double>{
		static_cast<double>(crop.left) / full_image->size().width,
		static_cast<double>(crop.top) / full_image->size().height,
		static_cast<double>(cropped_image->size().width) / full_image->size().width,
		static_cast<double>(cropped_image->size().height) / full_image->size().height
	};

	only_text()->emit_bitmap(_sequence.video[_next].period, cropped_image, rectangle);

	++_next;

	update_position();
	return false;
}


void
FCPXMLDecoder::seek(dcpomatic::ContentTime time, bool accurate)
{
	/* It's worth back-tracking a little here as decoding is cheap and it's nice if we don't miss
	   too many subtitles when seeking.
	*/
	time -= dcpomatic::ContentTime::from_seconds(5);
	if (time < dcpomatic::ContentTime()) {
		time = {};
	}

	Decoder::seek(time, accurate);

	_next = 0;
	while (_next < static_cast<int>(_sequence.video.size()) && _sequence.video[_next].period.from < time) {
		++_next;
	}

	update_position();
}


void
FCPXMLDecoder::update_position()
{
	if (_next < static_cast<int>(_sequence.video.size())) {
		only_text()->maybe_set_position(_sequence.video[_next].period.from);
	}
}

