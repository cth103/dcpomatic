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
#include "cross.h"
#include "font.h"
#include <cairomm/cairomm.h>
#include <pangomm.h>
#include <boost/foreach.hpp>
#include <iostream>

using std::list;
using std::cout;
using std::string;
using std::min;
using std::max;
using std::pair;
using std::cerr;
using std::make_pair;
using boost::shared_ptr;
using boost::optional;

static FcConfig* fc_config = 0;
static list<pair<boost::filesystem::path, string> > fc_config_fonts;

static PositionImage
render_subtitle (dcp::SubtitleString const & subtitle, list<shared_ptr<Font> > fonts, dcp::Size target)
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

	shared_ptr<Image> image (new Image (AV_PIX_FMT_RGBA, dcp::Size (target.width, height), false));
	image->make_black ();

	Cairo::RefPtr<Cairo::ImageSurface> surface = Cairo::ImageSurface::create (
		image->data()[0],
		Cairo::FORMAT_ARGB32,
		image->size().width,
		image->size().height,
		Cairo::ImageSurface::format_stride_for_width (Cairo::FORMAT_ARGB32, image->size().width)
		);

	Cairo::RefPtr<Cairo::Context> context = Cairo::Context::create (surface);

	if (!fc_config) {
		fc_config = FcConfigCreate ();
	}

	boost::filesystem::path font_file;
	try {
		font_file = shared_path () / "LiberationSans-Regular.ttf";
	} catch (boost::filesystem::filesystem_error& e) {
		/* Hack: try the debian/ubuntu location if getting the shared path failed */
		font_file = "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf";
	}

	BOOST_FOREACH (shared_ptr<Font> i, fonts) {
		if (i->id() == subtitle.font() && i->file ()) {
			font_file = i->file().get ();
		}
	}

	list<pair<boost::filesystem::path, string> >::const_iterator existing = fc_config_fonts.begin ();
	while (existing != fc_config_fonts.end() && existing->first != font_file) {
		++existing;
	}

	string font_name;
	if (existing != fc_config_fonts.end ()) {
		font_name = existing->second;
	} else {
		/* Make this font available to DCP-o-matic */
		FcConfigAppFontAddFile (fc_config, reinterpret_cast<FcChar8 const *> (font_file.string().c_str ()));

		FcPattern* pattern = FcPatternBuild (0, FC_FILE, FcTypeString, font_file.string().c_str(), static_cast<char *> (0));
		FcObjectSet* object_set = FcObjectSetBuild (FC_FAMILY, FC_STYLE, FC_LANG, FC_FILE, static_cast<char *> (0));
		FcFontSet* font_set = FcFontList (fc_config, pattern, object_set);
		if (font_set) {
			for (int i = 0; i < font_set->nfont; ++i) {
				FcPattern* font = font_set->fonts[i];
				FcChar8* file;
				FcChar8* family;
				FcChar8* style;
				if (
					FcPatternGetString (font, FC_FILE, 0, &file) == FcResultMatch &&
					FcPatternGetString (font, FC_FAMILY, 0, &family) == FcResultMatch &&
					FcPatternGetString (font, FC_STYLE, 0, &style) == FcResultMatch
					) {
					font_name = reinterpret_cast<char const *> (family);
				}
			}

			FcFontSetDestroy (font_set);
		}

		FcObjectSetDestroy (object_set);
		FcPatternDestroy (pattern);

		fc_config_fonts.push_back (make_pair (font_file, font_name));
	}

	FcConfigSetCurrent (fc_config);

	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (context);

	layout->set_alignment (Pango::ALIGN_LEFT);

	context->set_line_width (1);

	/* Render the subtitle at the top left-hand corner of image */

	Pango::FontDescription font (font_name);
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

	int x = 0;
	switch (subtitle.h_align ()) {
	case dcp::HALIGN_LEFT:
		/* h_position is distance between left of frame and left of subtitle */
		x = subtitle.h_position() * target.width;
		break;
	case dcp::HALIGN_CENTER:
		/* h_position is distance between centre of frame and centre of subtitle */
		x = (0.5 + subtitle.h_position()) * target.width - layout_width / (PANGO_SCALE * 2);
		break;
	case dcp::HALIGN_RIGHT:
		/* h_position is distance between right of frame and right of subtitle */
		x = (1.0 - subtitle.h_position()) * target.width - layout_width / PANGO_SCALE;
		break;
	}

	int y = 0;
	switch (subtitle.v_align ()) {
	case dcp::VALIGN_TOP:
		/* v_position is distance between top of frame and top of subtitle */
		y = subtitle.v_position() * target.height;
		break;
	case dcp::VALIGN_CENTER:
		/* v_position is distance between centre of frame and centre of subtitle */
		y = (0.5 + subtitle.v_position()) * target.height - layout_height / (PANGO_SCALE * 2);
		break;
	case dcp::VALIGN_BOTTOM:
		/* v_position is distance between bottom of frame and bottom of subtitle */
		y = (1.0 - subtitle.v_position()) * target.height - layout_height / PANGO_SCALE;
		break;
	}

	return PositionImage (image, Position<int> (x, y));
}

list<PositionImage>
render_subtitles (list<dcp::SubtitleString> subtitles, list<shared_ptr<Font> > fonts, dcp::Size target)
{
	list<PositionImage> images;
	BOOST_FOREACH (dcp::SubtitleString const & i, subtitles) {
		images.push_back (render_subtitle (i, fonts, target));
	}
	return images;
}
