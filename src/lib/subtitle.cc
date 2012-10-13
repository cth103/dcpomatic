/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

#include "subtitle.h"
#include "image.h"
#include "exceptions.h"
#include "film_state.h"

using namespace std;
using namespace boost;

Subtitle::Subtitle (AVSubtitle const & sub)
{
	/* subtitle PTS in seconds */
	float const packet_time = (sub.pts / AV_TIME_BASE) + float (sub.pts % AV_TIME_BASE) / 1e6;
	
	/* hence start time for this sub */
	_from = packet_time + (double (sub.start_display_time) / 1e3);
	_to = packet_time + (double (sub.end_display_time) / 1e3);

	for (unsigned int i = 0; i < sub.num_rects; ++i) {
		_images.push_back (shared_ptr<SubtitleImage> (new SubtitleImage (sub.rects[i])));
	}
}

/** @param t Time in seconds from the start of the film */
bool
Subtitle::displayed_at (double t)
{
	return t >= _from && t <= _to;
}

SubtitleImage::SubtitleImage (AVSubtitleRect const * rect)
	: _position (rect->x, rect->y)
	, _image (new SimpleImage (PIX_FMT_RGBA, Size (rect->w, rect->h)))
{
	if (rect->type != SUBTITLE_BITMAP) {
		throw DecodeError ("non-bitmap subtitles not yet supported");
	}

	/* Start of the first line in the subtitle */
	uint8_t* sub_p = rect->pict.data[0];
	/* sub_p looks up into a RGB palette which is here */
	uint32_t const * palette = (uint32_t *) rect->pict.data[1];
	/* Start of the output data */
	uint32_t* out_p = (uint32_t *) _image->data()[0];
	
	for (int y = 0; y < rect->h; ++y) {
		uint8_t* sub_line_p = sub_p;
		for (int x = 0; x < rect->w; ++x) {
			*out_p++ = palette[*sub_line_p++];
		}
		sub_p += rect->pict.linesize[0];
	}
}

SubtitleTransform
subtitle_transform (
	int target_base_width, int target_base_height,
	float target_x_scale, float target_y_scale,
	Position sub_pos, int sub_width, int sub_height,
	shared_ptr<FilmState> fs
	)
{
	SubtitleTransform tx;

	Rectangle sub_area (sub_pos.x, sub_pos.y + fs->subtitle_offset, sub_width, sub_height);
	
	Rectangle cropped_target_area (
		fs->crop.left,
		fs->crop.top,
		target_base_width - (fs->crop.left + fs->crop.right),
		target_base_height - (fs->crop.top + fs->crop.bottom)
		);
	
	Rectangle cropped_sub_area = sub_area.intersection (cropped_target_area);
	
	tx.crop.x = cropped_sub_area.x - sub_area.x;
	tx.crop.y = cropped_sub_area.y - sub_area.y;
	tx.crop.w = cropped_sub_area.w;
	tx.crop.h = cropped_sub_area.h;

	tx.transformed.w = cropped_sub_area.w * target_x_scale * fs->subtitle_scale;
	tx.transformed.h = cropped_sub_area.h * target_y_scale * fs->subtitle_scale;

	tx.transformed.x = target_x_scale * ((sub_area.x - fs->crop.left) + (cropped_sub_area.w * (1 - fs->subtitle_scale) / 2));
	tx.transformed.y = target_y_scale * ((sub_area.y - fs->crop.top) + (cropped_sub_area.h * (1 - fs->subtitle_scale) / 2));

	return tx;
}
