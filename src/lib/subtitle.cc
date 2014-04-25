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

#include "subtitle.h"
#include "subtitle_content.h"
#include "piece.h"
#include "image.h"
#include "scaler.h"
#include "film.h"

using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::weak_ptr;

Subtitle::Subtitle (shared_ptr<const Film> film, libdcp::Size video_container_size, weak_ptr<Piece> weak_piece, shared_ptr<Image> image, dcpomatic::Rect<double> rect, Time from, Time to)
	: _piece (weak_piece)
	, _in_image (image)
	, _in_rect (rect)
	, _in_from (from)
	, _in_to (to)
{
	update (film, video_container_size);
}

void
Subtitle::update (shared_ptr<const Film> film, libdcp::Size video_container_size)
{
	shared_ptr<Piece> piece = _piece.lock ();
	if (!piece) {
		return;
	}

	if (!_in_image) {
		_out_image.reset ();
		return;
	}

	shared_ptr<SubtitleContent> sc = dynamic_pointer_cast<SubtitleContent> (piece->content);
	assert (sc);

	dcpomatic::Rect<double> in_rect = _in_rect;
	libdcp::Size scaled_size;

	in_rect.x += sc->subtitle_x_offset ();
	in_rect.y += sc->subtitle_y_offset ();

	/* We will scale the subtitle up to fit _video_container_size, and also by the additional subtitle_scale */
	scaled_size.width = in_rect.width * video_container_size.width * sc->subtitle_scale ();
	scaled_size.height = in_rect.height * video_container_size.height * sc->subtitle_scale ();

	/* Then we need a corrective translation, consisting of two parts:
	 *
	 * 1.  that which is the result of the scaling of the subtitle by _video_container_size; this will be
	 *     rect.x * _video_container_size.width and rect.y * _video_container_size.height.
	 *
	 * 2.  that to shift the origin of the scale by subtitle_scale to the centre of the subtitle; this will be
	 *     (width_before_subtitle_scale * (1 - subtitle_scale) / 2) and
	 *     (height_before_subtitle_scale * (1 - subtitle_scale) / 2).
	 *
	 * Combining these two translations gives these expressions.
	 */
	
	_out_position.x = rint (video_container_size.width * (in_rect.x + (in_rect.width * (1 - sc->subtitle_scale ()) / 2)));
	_out_position.y = rint (video_container_size.height * (in_rect.y + (in_rect.height * (1 - sc->subtitle_scale ()) / 2)));
	
	_out_image = _in_image->scale (
		scaled_size,
		Scaler::from_id ("bicubic"),
		_in_image->pixel_format (),
		true
		);

	/* XXX: hack */
	Time from = _in_from;
	Time to = _in_to;
	shared_ptr<VideoContent> vc = dynamic_pointer_cast<VideoContent> (piece->content);
	if (vc) {
		from = rint (from * vc->video_frame_rate() / film->video_frame_rate());
		to = rint (to * vc->video_frame_rate() / film->video_frame_rate());
	}
	
	_out_from = from + piece->content->position ();
	_out_to = to + piece->content->position ();

	check_out_to ();
}

bool
Subtitle::covers (Time t) const
{
	return _out_from <= t && t <= _out_to;
}

void
Subtitle::check_out_to ()
{
	if (_stop && _out_to > _stop.get ()) {
		_out_to = _stop.get ();
	}
}
