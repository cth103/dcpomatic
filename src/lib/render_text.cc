/*
    Copyright (C) 2014-2016 Carl Hetherington <cth@carlh.net>

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

#include "render_text.h"
#include "types.h"
#include "image.h"
#include "cross.h"
#include "font.h"
#include "dcpomatic_assert.h"
#include "warnings.h"
#include "util.h"
#include <dcp/raw_convert.h>
#include <fontconfig/fontconfig.h>
#include <cairomm/cairomm.h>
DCPOMATIC_DISABLE_WARNINGS
#include <pangomm.h>
DCPOMATIC_ENABLE_WARNINGS
#include <pango/pangocairo.h>
#ifndef DCPOMATIC_HAVE_SHOW_IN_CAIRO_CONTEXT
#include <pango/pangocairo.h>
#endif
#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
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
using boost::algorithm::replace_all;
using namespace dcpomatic;

static FcConfig* fc_config = 0;
static list<pair<boost::filesystem::path, string> > fc_config_fonts;

string
marked_up (list<StringText> subtitles, int target_height, float fade_factor)
{
	string out;

	BOOST_FOREACH (StringText const & i, subtitles) {
		out += "<span ";
		if (i.italic()) {
			out += "style=\"italic\" ";
		}
		if (i.bold()) {
			out += "weight=\"bold\" ";
		}
		if (i.underline()) {
			out += "underline=\"single\" ";
		}
		out += "size=\"" + dcp::raw_convert<string>(i.size_in_pixels(target_height) * 72 * 1024 / 96) + "\" ";
		/* Between 1-65535 inclusive, apparently... */
		out += "alpha=\"" + dcp::raw_convert<string>(int(floor(fade_factor * 65534)) + 1) + "\" ";
		out += "color=\"#" + i.colour().to_rgb_string() + "\">";

		string t = i.text();
		replace_all(t, "&", "&amp;");
		out += t;

		out += "</span>";
	}

	return out;
}

static void
set_source_rgba (Cairo::RefPtr<Cairo::Context> context, dcp::Colour colour, float fade_factor)
{
	context->set_source_rgba (float(colour.r) / 255, float(colour.g) / 255, float(colour.b) / 255, fade_factor);
}


static shared_ptr<Image>
create_image (dcp::Size size)
{
	/* FFmpeg BGRA means first byte blue, second byte green, third byte red, fourth byte alpha */
	shared_ptr<Image> image (new Image(AV_PIX_FMT_BGRA, size, false));
	image->make_black ();
	return image;
}


static Cairo::RefPtr<Cairo::ImageSurface>
create_surface (shared_ptr<Image> image)
{
#ifdef DCPOMATIC_HAVE_FORMAT_STRIDE_FOR_WIDTH
	Cairo::RefPtr<Cairo::ImageSurface> surface = Cairo::ImageSurface::create (
		image->data()[0],
		Cairo::FORMAT_ARGB32,
		image->size().width,
		image->size().height,
		/* Cairo ARGB32 means first byte blue, second byte green, third byte red, fourth byte alpha */
		Cairo::ImageSurface::format_stride_for_width (Cairo::FORMAT_ARGB32, image->size().width)
		);
#else
	/* Centos 5 does not have Cairo::ImageSurface::format_stride_for_width, so just use width * 4
	   which I hope is safe (if slow)
	*/
	Cairo::RefPtr<Cairo::ImageSurface> surface = Cairo::ImageSurface::create (
		image->data()[0],
		Cairo::FORMAT_ARGB32,
		image->size().width,
		image->size().height,
		image->size().width * 4
		);
#endif

	return surface;
}


static string
setup_font (StringText const& subtitle, list<shared_ptr<Font> > const& fonts)
{
	if (!fc_config) {
		fc_config = FcInitLoadConfig ();
	}

	optional<boost::filesystem::path> font_file = default_font_file ();

	BOOST_FOREACH (shared_ptr<Font> i, fonts) {
		if (i->id() == subtitle.font() && i->file()) {
			font_file = i->file ();
		}
	}

	list<pair<boost::filesystem::path, string> >::const_iterator existing = fc_config_fonts.begin ();
	while (existing != fc_config_fonts.end() && existing->first != *font_file) {
		++existing;
	}

	string font_name;
	if (existing != fc_config_fonts.end ()) {
		font_name = existing->second;
	} else {
		/* Make this font available to DCP-o-matic */
		FcConfigAppFontAddFile (fc_config, reinterpret_cast<FcChar8 const *>(font_file->string().c_str()));
		FcPattern* pattern = FcPatternBuild (
			0, FC_FILE, FcTypeString, font_file->string().c_str(), static_cast<char *> (0)
			);
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

		fc_config_fonts.push_back (make_pair(*font_file, font_name));
	}

	FcConfigSetCurrent (fc_config);
	return font_name;
}


static float
calculate_fade_factor (StringText const& first, DCPTime time, int frame_rate)
{
	float fade_factor = 1;

	/* Round the fade start/end to the nearest frame start.  Otherwise if a subtitle starts just after
	   the start of a frame it will be faded out.
	*/
	DCPTime const fade_in_start = DCPTime::from_seconds(first.in().as_seconds()).round(frame_rate);
	DCPTime const fade_in_end = fade_in_start + DCPTime::from_seconds (first.fade_up_time().as_seconds ());
	DCPTime const fade_out_end =  DCPTime::from_seconds (first.out().as_seconds()).round(frame_rate);
	DCPTime const fade_out_start = fade_out_end - DCPTime::from_seconds (first.fade_down_time().as_seconds ());

	if (fade_in_start <= time && time <= fade_in_end && fade_in_start != fade_in_end) {
		fade_factor *= DCPTime(time - fade_in_start).seconds() / DCPTime(fade_in_end - fade_in_start).seconds();
	}
	if (fade_out_start <= time && time <= fade_out_end && fade_out_start != fade_out_end) {
		fade_factor *= 1 - DCPTime(time - fade_out_start).seconds() / DCPTime(fade_out_end - fade_out_start).seconds();
	}
	if (time < fade_in_start || time > fade_out_end) {
		fade_factor = 0;
	}

	return fade_factor;
}


static int
x_position (StringText const& first, int target_width, int layout_width)
{
	int x = 0;
	switch (first.h_align ()) {
	case dcp::HALIGN_LEFT:
		/* h_position is distance between left of frame and left of subtitle */
		x = first.h_position() * target_width;
		break;
	case dcp::HALIGN_CENTER:
		/* h_position is distance between centre of frame and centre of subtitle */
		x = (0.5 + first.h_position()) * target_width - layout_width / 2;
		break;
	case dcp::HALIGN_RIGHT:
		/* h_position is distance between right of frame and right of subtitle */
		x = (1.0 - first.h_position()) * target_width - layout_width;
		break;
	}

	return x;
}



static int
y_position (StringText const& first, int target_height, int layout_height)
{
	int y = 0;
	switch (first.v_align ()) {
	case dcp::VALIGN_TOP:
		/* SMPTE says that v_position is the distance between top
		   of frame and top of subtitle, but this doesn't always seem to be
		   the case in practice; Gunnar Ásgeirsson's Dolby server appears
		   to put VALIGN_TOP subs with v_position as the distance between top
		   of frame and bottom of subtitle.
		*/
		y = first.v_position() * target_height - layout_height;
		break;
	case dcp::VALIGN_CENTER:
		/* v_position is distance between centre of frame and centre of subtitle */
		y = (0.5 + first.v_position()) * target_height - layout_height / 2;
		break;
	case dcp::VALIGN_BOTTOM:
		/* v_position is distance between bottom of frame and bottom of subtitle */
		y = (1.0 - first.v_position()) * target_height - layout_height;
		break;
	}

	return y;
}


static void
setup_layout (Glib::RefPtr<Pango::Layout> layout, string font_name, string markup)
{
	layout->set_alignment (Pango::ALIGN_LEFT);
	Pango::FontDescription font (font_name);
	layout->set_font_description (font);
	layout->set_markup (markup);
}

/** Create a Pango layout using a dummy context which we can use to calculate the size
 *  of the text we will render.  Then we can transfer the layout over to the real context
 *  for the actual render.
 */
static Glib::RefPtr<Pango::Layout>
create_layout()
{
	PangoFontMap* c_font_map = pango_cairo_font_map_new ();
	Glib::RefPtr<Pango::FontMap> font_map = Glib::wrap (c_font_map);
	PangoContext* c_context = pango_font_map_create_context (c_font_map);
	Glib::RefPtr<Pango::Context> context = Glib::wrap (c_context);
	return Pango::Layout::create (context);
}


/** @param subtitles A list of subtitles that are all on the same line,
 *  at the same time and with the same fade in/out.
 */
static PositionImage
render_line (list<StringText> subtitles, list<shared_ptr<Font> > fonts, dcp::Size target, DCPTime time, int frame_rate)
{
	/* XXX: this method can only handle italic / bold changes mid-line,
	   nothing else yet.
	*/

	DCPOMATIC_ASSERT (!subtitles.empty ());
	StringText const& first = subtitles.front ();

	string const font_name = setup_font (first, fonts);
	float const fade_factor = calculate_fade_factor (first, time, frame_rate);
	string const markup = marked_up (subtitles, target.height, fade_factor);
	Glib::RefPtr<Pango::Layout> layout = create_layout ();
	setup_layout (layout, font_name, markup);
	dcp::Size size;
	layout->get_pixel_size (size.width, size.height);

	/* Calculate x and y scale factors.  These are only used to stretch
	   the font away from its normal aspect ratio.
	*/
	float x_scale = 1;
	float y_scale = 1;
	if (fabs (first.aspect_adjust() - 1.0) > dcp::ASPECT_ADJUST_EPSILON) {
		if (first.aspect_adjust() < 1) {
			x_scale = max (0.25f, first.aspect_adjust ());
			y_scale = 1;
		} else {
			x_scale = 1;
			y_scale = 1 / min (4.0f, first.aspect_adjust ());
		}
	}

	float const border_width = first.effect() == dcp::BORDER ? (first.outline_width * target.width / 2048.0) : 0;
	size.width += 2 * ceil (border_width);
	size.height += 2 * ceil (border_width);

	size.width *= x_scale;
	size.height *= y_scale;

	/* Shuffle the subtitle over by the border width (if we have any) so it's not cut off */
	int const x_offset = ceil (border_width);
	/* Move down a bit so that accents on capital letters can be seen */
	int const y_offset = target.height / 100.0;

	size.width += x_offset;
	size.height += y_offset;

	shared_ptr<Image> image = create_image (size);
	Cairo::RefPtr<Cairo::Surface> surface = create_surface (image);
	Cairo::RefPtr<Cairo::Context> context = Cairo::Context::create (surface);

	context->set_line_width (1);
	context->scale (x_scale, y_scale);
	layout->update_from_cairo_context (context);

	if (first.effect() == dcp::SHADOW) {
		/* Drop-shadow effect */
		set_source_rgba (context, first.effect_colour(), fade_factor);
		context->move_to (x_offset + 4, y_offset + 4);
		layout->add_to_cairo_context (context);
		context->fill ();
	}

	if (first.effect() == dcp::BORDER) {
		/* Border effect */
		set_source_rgba (context, first.effect_colour(), fade_factor);
		context->set_line_width (border_width);
		context->set_line_join (Cairo::LINE_JOIN_ROUND);
		context->move_to (x_offset, y_offset);
		layout->add_to_cairo_context (context);
		context->stroke ();
	}

	/* The actual subtitle */

	set_source_rgba (context, first.colour(), fade_factor);

	context->move_to (x_offset, y_offset);
	layout->add_to_cairo_context (context);
	context->fill ();

	context->set_line_width (0.5);
	context->move_to (x_offset, y_offset);
	layout->add_to_cairo_context (context);
	context->stroke ();

	int const x = x_position (first, target.width, size.width);
	int const y = y_position (first, target.height, size.height);
	return PositionImage (image, Position<int>(max (0, x), max(0, y)));
}


/** @param time Time of the frame that these subtitles are going on.
 *  @param target Size of the container that this subtitle will end up in.
 *  @param frame_rate DCP frame rate.
 */
list<PositionImage>
render_text (list<StringText> subtitles, list<shared_ptr<Font> > fonts, dcp::Size target, DCPTime time, int frame_rate)
{
	list<StringText> pending;
	list<PositionImage> images;

	BOOST_FOREACH (StringText const & i, subtitles) {
		if (!pending.empty() && (i.v_align() != pending.back().v_align() || fabs(i.v_position() - pending.back().v_position()) > 1e-4)) {
			images.push_back (render_line (pending, fonts, target, time, frame_rate));
			pending.clear ();
		}
		pending.push_back (i);
	}

	if (!pending.empty ()) {
		images.push_back (render_line (pending, fonts, target, time, frame_rate));
	}

	return images;
}
