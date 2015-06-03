/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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

#include "render_subtitles.h"
#include "types.h"
#include "image.h"
#include <cairomm/cairomm.h>
#include <pangomm.h>
#include <boost/foreach.hpp>

using std::list;
using std::cout;
using std::string;
using std::min;
using std::max;
using std::pair;
using boost::shared_ptr;
using boost::optional;

static PositionImage
render_subtitle (dcp::SubtitleString const & subtitle, dcp::Size target)
{
	/* Calculate x and y scale factors.  These are only used to stretch
	   the font away from its normal aspect ratio.
	*/
	float xscale = 1;
	float yscale = 1;
	if (fabs (subtitle.aspect_adjust() - 1.0) > dcp::ASPECT_ADJUST_EPSILON) {
		if (subtitle.aspect_adjust() < 1) {
			xscale = max (0.25f, subtitle.aspect_adjust ());
			yscale = 1;
		} else {
			xscale = 1;
			yscale = 1 / min (4.0f, subtitle.aspect_adjust ());
		}
	}

	/* Make an empty bitmap as wide as target and at
	   least tall enough for this subtitle.
	*/

	/* Basic guess on height... */
	int height = subtitle.size() * target.height / (11 * 72);
	/* ...scaled... */
	height *= yscale;
	/* ...and add a bit more for luck */
	height += target.height / 11;

	shared_ptr<Image> image (new Image (PIX_FMT_RGBA, dcp::Size (target.width, height), false));
	image->make_black ();

	Cairo::RefPtr<Cairo::ImageSurface> surface = Cairo::ImageSurface::create (
		image->data()[0],
		Cairo::FORMAT_ARGB32,
		image->size().width,
		image->size().height,
		Cairo::ImageSurface::format_stride_for_width (Cairo::FORMAT_ARGB32, image->size().width)
		);
	
	Cairo::RefPtr<Cairo::Context> context = Cairo::Context::create (surface);
	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (context);

	layout->set_alignment (Pango::ALIGN_LEFT);

	context->set_line_width (1);

	/* Render the subtitle at the top left-hand corner of image */

	Pango::FontDescription font (subtitle.font().get_value_or ("Arial"));
	font.set_absolute_size (subtitle.size_in_pixels (target.height) * PANGO_SCALE);
	if (subtitle.italic ()) {
		font.set_style (Pango::STYLE_ITALIC);
	}
	layout->set_font_description (font);
	layout->set_text (subtitle.text ());
	
	/* Compute fade factor */
	/* XXX */
	float fade_factor = 1;

	layout->update_from_cairo_context (context);
		
	context->scale (xscale, yscale);

	if (subtitle.effect() == dcp::SHADOW) {
		/* Drop-shadow effect */
		dcp::Colour const ec = subtitle.effect_colour ();
		context->set_source_rgba (float(ec.r) / 255, float(ec.g) / 255, float(ec.b) / 255, fade_factor);
		context->move_to (4, 4);
		layout->add_to_cairo_context (context);
		context->fill ();
	}

	/* The actual subtitle */

	dcp::Colour const c = subtitle.colour ();
	context->set_source_rgba (float(c.r) / 255, float(c.g) / 255, float(c.b) / 255, fade_factor);
	context->move_to (0, 0);
	layout->add_to_cairo_context (context);
	context->fill ();

	if (subtitle.effect() == dcp::BORDER) {
		/* Border effect */
		dcp::Colour ec = subtitle.effect_colour ();
		context->set_source_rgba (float(ec.r) / 255, float(ec.g) / 255, float(ec.b) / 255, fade_factor);
		context->move_to (0, 0);
		layout->add_to_cairo_context (context);
		context->stroke ();
	}
	
	int layout_width;
	int layout_height;
	layout->get_size (layout_width, layout_height);

	int y = 0;
	switch (subtitle.v_align ()) {
	case dcp::TOP:
		/* v_position is distance between top of frame and top of subtitle */
		y = subtitle.v_position() * target.height;
		break;
	case dcp::CENTER:
		/* v_position is distance between centre of frame and centre of subtitle */
		y = 0.5 + subtitle.v_position() * target.height - (layout_height / (PANGO_SCALE * 2));
		break;
	case dcp::BOTTOM:
		/* v_position is distance between bottom of frame and bottom of subtitle */
		y = (1.0 - subtitle.v_position()) * target.height - layout_height / PANGO_SCALE;
		break;
	}

	return PositionImage (image, Position<int> ((image->size().width - layout_width * xscale / PANGO_SCALE) / 2, y));
}

list<PositionImage>
render_subtitles (list<dcp::SubtitleString> subtitles, dcp::Size target)
{
	list<PositionImage> images;
	BOOST_FOREACH (dcp::SubtitleString const & i, subtitles) {
		images.push_back (render_subtitle (i, target));
	}
	return images;
}
