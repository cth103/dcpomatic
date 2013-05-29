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

/** @file  src/subtitle.cc
 *  @brief Representations of subtitles.
 */

#include "subtitle.h"
#include "image.h"
#include "exceptions.h"

#include "i18n.h"

using namespace std;
using namespace boost;
using libdcp::Size;

/** Construct a TimedSubtitle.  This is a subtitle image, position,
 *  and a range of time over which it should be shown.
 *  @param sub AVSubtitle to read.
 */
TimedSubtitle::TimedSubtitle (AVSubtitle const & sub)
{
	assert (sub.num_rects > 0);
	
	/* Subtitle PTS in seconds (within the source, not taking into account any of the
	   source that we may have chopped off for the DCP)
	*/
	double const packet_time = static_cast<double> (sub.pts) / AV_TIME_BASE;
	
	/* hence start time for this sub */
	_from = packet_time + (double (sub.start_display_time) / 1e3);
	_to = packet_time + (double (sub.end_display_time) / 1e3);

	if (sub.num_rects > 1) {
		throw DecodeError (_("multi-part subtitles not yet supported"));
	}

	AVSubtitleRect const * rect = sub.rects[0];

	if (rect->type != SUBTITLE_BITMAP) {
		throw DecodeError (_("non-bitmap subtitles not yet supported"));
	}
	
	shared_ptr<Image> image (new SimpleImage (PIX_FMT_RGBA, libdcp::Size (rect->w, rect->h), true));

	/* Start of the first line in the subtitle */
	uint8_t* sub_p = rect->pict.data[0];
	/* sub_p looks up into a RGB palette which is here */
	uint32_t const * palette = (uint32_t *) rect->pict.data[1];
	/* Start of the output data */
	uint32_t* out_p = (uint32_t *) image->data()[0];
	
	for (int y = 0; y < rect->h; ++y) {
		uint8_t* sub_line_p = sub_p;
		uint32_t* out_line_p = out_p;
		for (int x = 0; x < rect->w; ++x) {
			*out_line_p++ = palette[*sub_line_p++];
		}
		sub_p += rect->pict.linesize[0];
		out_p += image->stride()[0] / sizeof (uint32_t);
	}

	_subtitle.reset (new Subtitle (Position (rect->x, rect->y), image));
}	

/** @param t Time in seconds from the start of the source */
bool
TimedSubtitle::displayed_at (double t) const
{
	return t >= _from && t <= _to;
}

/** Construct a subtitle, which is an image and a position.
 *  @param p Position within the (uncropped) source frame.
 *  @param i Image of the subtitle (should be RGBA).
 */
Subtitle::Subtitle (Position p, shared_ptr<Image> i)
	: _position (p)
	, _image (i)
{

}

/** Given the area of a subtitle, work out the area it should
 *  take up when its video frame is scaled up, and it is optionally
 *  itself scaled and offset.
 *  @param target_x_scale the x scaling of the video frame that the subtitle is in.
 *  @param target_y_scale the y scaling of the video frame that the subtitle is in.
 *  @param sub_area The area of the subtitle within the original source.
 *  @param subtitle_offset y offset to apply to the subtitle position (+ve is down)
 *  in the coordinate space of the source.
 *  @param subtitle_scale scaling factor to apply to the subtitle image.
 */
dvdomatic::Rect
subtitle_transformed_area (
	float target_x_scale, float target_y_scale,
	dvdomatic::Rect sub_area, int subtitle_offset, float subtitle_scale
	)
{
	dvdomatic::Rect tx;

	sub_area.y += subtitle_offset;

	/* We will scale the subtitle by the same amount as the video frame, and also by the additional
	   subtitle_scale
	*/
	tx.width = sub_area.width * target_x_scale * subtitle_scale;
	tx.height = sub_area.height * target_y_scale * subtitle_scale;

	/* Then we need a corrective translation, consisting of two parts:
	 *
	 * 1.  that which is the result of the scaling of the subtitle by target_x_scale and target_y_scale; this will be
	 *     sub_area.x * target_x_scale and sub_area.y * target_y_scale.
	 *
	 * 2.  that to shift the origin of the scale by subtitle_scale to the centre of the subtitle; this will be
	 *     (width_before_subtitle_scale * (1 - subtitle_scale) / 2) and
	 *     (height_before_subtitle_scale * (1 - subtitle_scale) / 2).
	 *
	 * Combining these two translations gives these expressions.
	 */
	
	tx.x = rint (target_x_scale * (sub_area.x + (sub_area.width * (1 - subtitle_scale) / 2)));
	tx.y = rint (target_y_scale * (sub_area.y + (sub_area.height * (1 - subtitle_scale) / 2)));

	return tx;
}

/** @return area that this subtitle takes up, in the original uncropped source's coordinate space */
dvdomatic::Rect
Subtitle::area () const
{
	return dvdomatic::Rect (_position.x, _position.y, _image->size().width, _image->size().height);
}
