/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include <cairomm/cairomm.h>
#include <pangomm.h>
#include "render_subtitles.h"
#include "types.h"
#include "image.h"

using std::list;
using std::cout;
using std::string;
using std::min;
using std::max;
using std::pair;
using boost::shared_ptr;
using boost::optional;

static int
calculate_position (dcp::VAlign v_align, double v_position, int target_height, int offset)
{
	switch (v_align) {
	case dcp::TOP:
		return v_position * target_height - offset;
	case dcp::CENTER:
		return (0.5 + v_position) * target_height - offset;
	case dcp::BOTTOM:
		return (1.0 - v_position) * target_height - offset;
	}

	return 0;
}

PositionImage
render_subtitles (list<dcp::SubtitleString> subtitles, dcp::Size target)
{
	/* Estimate height that the subtitle image needs to be */
	optional<int> top;
	optional<int> bottom;
	for (list<dcp::SubtitleString>::const_iterator i = subtitles.begin(); i != subtitles.end(); ++i) {
		int const b = calculate_position (i->v_align(), i->v_position(), target.height, 0);
		int const t = b - i->size() * target.height / (11 * 72);

		top = min (top.get_value_or (t), t);
		bottom = max (bottom.get_value_or (b), b);
	}

	top = top.get() - 32;
	bottom = bottom.get() + 32;

	shared_ptr<Image> image (new Image (PIX_FMT_RGBA, dcp::Size (target.width, bottom.get() - top.get ()), false));
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

	layout->set_width (image->size().width * PANGO_SCALE);
	layout->set_alignment (Pango::ALIGN_CENTER);

	context->set_line_width (1);

	for (list<dcp::SubtitleString>::const_iterator i = subtitles.begin(); i != subtitles.end(); ++i) {
		Pango::FontDescription font (i->font().get_value_or ("Arial"));
		font.set_absolute_size (i->size_in_pixels (target.height) * PANGO_SCALE);
		if (i->italic ()) {
			font.set_style (Pango::STYLE_ITALIC);
		}
		layout->set_font_description (font);
		layout->set_text (i->text ());

		/* Compute fade factor */
		/* XXX */
		float fade_factor = 1;

		layout->update_from_cairo_context (context);
		
		/* Work out position */

		int const x = 0;
		int const y = calculate_position (i->v_align (), i->v_position (), target.height, (layout->get_baseline() / PANGO_SCALE) + top.get ());

		if (i->effect() == dcp::SHADOW) {
			/* Drop-shadow effect */
			dcp::Colour const ec = i->effect_colour ();
			context->set_source_rgba (float(ec.r) / 255, float(ec.g) / 255, float(ec.b) / 255, fade_factor);
			context->move_to (x + 4, y + 4);
			layout->add_to_cairo_context (context);
			context->fill ();
		}

		/* The actual subtitle */
		context->move_to (x, y);
		dcp::Colour const c = i->colour ();
		context->set_source_rgba (float(c.r) / 255, float(c.g) / 255, float(c.b) / 255, fade_factor);
		layout->add_to_cairo_context (context);
		context->fill ();

		if (i->effect() == dcp::BORDER) {
			/* Border effect */
			context->move_to (x, y);
			dcp::Colour ec = i->effect_colour ();
			context->set_source_rgba (float(ec.r) / 255, float(ec.g) / 255, float(ec.b) / 255, fade_factor);
			layout->add_to_cairo_context (context);
			context->stroke ();
		}
	}

	return PositionImage (image, Position<int> (0, top.get ()));
}

