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
#include <dcp/raw_convert.h>
#include <fontconfig/fontconfig.h>
#include <cairomm/cairomm.h>
#include <pangomm.h>
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

static FcConfig* fc_config = 0;
static list<pair<FontFiles, string> > fc_config_fonts;

string
marked_up (list<PlainText> subtitles, int target_height, float fade_factor)
{
	string out;

	BOOST_FOREACH (PlainText const & i, subtitles) {
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

/** @param subtitles A list of subtitles that are all on the same line,
 *  at the same time and with the same fade in/out.
 */
static PositionImage
render_line (list<PlainText> subtitles, list<shared_ptr<Font> > fonts, dcp::Size target, DCPTime time, int frame_rate)
{
	/* XXX: this method can only handle italic / bold changes mid-line,
	   nothing else yet.
	*/

	DCPOMATIC_ASSERT (!subtitles.empty ());

	/* Calculate x and y scale factors.  These are only used to stretch
	   the font away from its normal aspect ratio.
	*/
	float xscale = 1;
	float yscale = 1;
	if (fabs (subtitles.front().aspect_adjust() - 1.0) > dcp::ASPECT_ADJUST_EPSILON) {
		if (subtitles.front().aspect_adjust() < 1) {
			xscale = max (0.25f, subtitles.front().aspect_adjust ());
			yscale = 1;
		} else {
			xscale = 1;
			yscale = 1 / min (4.0f, subtitles.front().aspect_adjust ());
		}
	}

	/* Make an empty bitmap as wide as target and at
	   least tall enough for this subtitle.
	*/

	int largest = 0;
	BOOST_FOREACH (dcp::SubtitleString const & i, subtitles) {
		largest = max (largest, i.size());
	}
	/* Basic guess on height... */
	int height = largest * target.height / (11 * 72);
	/* ...scaled... */
	height *= yscale;
	/* ...and add a bit more for luck */
	height += target.height / 11;

	/* FFmpeg BGRA means first byte blue, second byte green, third byte red, fourth byte alpha */
	shared_ptr<Image> image (new Image (AV_PIX_FMT_BGRA, dcp::Size (target.width, height), false));
	image->make_black ();

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

	Cairo::RefPtr<Cairo::Context> context = Cairo::Context::create (surface);

	if (!fc_config) {
		fc_config = FcConfigCreate ();
	}

	FontFiles font_files;

	try {
		font_files.set (FontFiles::NORMAL, shared_path () / "LiberationSans-Regular.ttf");
		font_files.set (FontFiles::ITALIC, shared_path () / "LiberationSans-Italic.ttf");
		font_files.set (FontFiles::BOLD, shared_path () / "LiberationSans-Bold.ttf");
	} catch (boost::filesystem::filesystem_error& e) {

	}

	/* Hack: try the debian/ubuntu locations if getting the shared path failed */

	if (!font_files.get(FontFiles::NORMAL) || !boost::filesystem::exists(font_files.get(FontFiles::NORMAL).get())) {
		font_files.set (FontFiles::NORMAL, "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf");
	}
	if (!font_files.get(FontFiles::ITALIC) || !boost::filesystem::exists(font_files.get(FontFiles::ITALIC).get())) {
		font_files.set (FontFiles::ITALIC, "/usr/share/fonts/truetype/liberation/LiberationSans-Italic.ttf");
	}
	if (!font_files.get(FontFiles::BOLD) || !boost::filesystem::exists(font_files.get(FontFiles::BOLD).get())) {
		font_files.set (FontFiles::BOLD, "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf");
	}

	BOOST_FOREACH (shared_ptr<Font> i, fonts) {
		if (i->id() == subtitles.front().font() && i->file(FontFiles::NORMAL)) {
			font_files = i->files ();
		}
	}

	list<pair<FontFiles, string> >::const_iterator existing = fc_config_fonts.begin ();
	while (existing != fc_config_fonts.end() && existing->first != font_files) {
		++existing;
	}

	string font_name;
	if (existing != fc_config_fonts.end ()) {
		font_name = existing->second;
	} else {
		/* Make this font available to DCP-o-matic */
		for (int i = 0; i < FontFiles::VARIANTS; ++i) {
			if (font_files.get(static_cast<FontFiles::Variant>(i))) {
				FcConfigAppFontAddFile (
					fc_config,
					reinterpret_cast<FcChar8 const *> (font_files.get(static_cast<FontFiles::Variant>(i)).get().string().c_str())
					);
			}
		}

		FcPattern* pattern = FcPatternBuild (
			0, FC_FILE, FcTypeString, font_files.get(FontFiles::NORMAL).get().string().c_str(), static_cast<char *> (0)
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

		fc_config_fonts.push_back (make_pair (font_files, font_name));
	}

	FcConfigSetCurrent (fc_config);

	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (context);

	layout->set_alignment (Pango::ALIGN_LEFT);

	context->set_line_width (1);
	context->set_antialias (Cairo::ANTIALIAS_GRAY);
	Cairo::FontOptions fo;
	context->get_font_options (fo);
	fo.set_antialias (Cairo::ANTIALIAS_GRAY);
	context->set_font_options (fo);

	/* Compute fade factor */
	float fade_factor = 1;

	/* Round the fade start/end to the nearest frame start.  Otherwise if a subtitle starts just after
	   the start of a frame it will be faded out.
	*/
	DCPTime const fade_in_start = DCPTime::from_seconds(subtitles.front().in().as_seconds()).round(frame_rate);
	DCPTime const fade_in_end = fade_in_start + DCPTime::from_seconds (subtitles.front().fade_up_time().as_seconds ());
	DCPTime const fade_out_end =  DCPTime::from_seconds (subtitles.front().out().as_seconds()).round(frame_rate);
	DCPTime const fade_out_start = fade_out_end - DCPTime::from_seconds (subtitles.front().fade_down_time().as_seconds ());

	if (fade_in_start <= time && time <= fade_in_end && fade_in_start != fade_in_end) {
		fade_factor *= DCPTime(time - fade_in_start).seconds() / DCPTime(fade_in_end - fade_in_start).seconds();
	}
	if (fade_out_start <= time && time <= fade_out_end && fade_out_start != fade_out_end) {
		fade_factor *= 1 - DCPTime(time - fade_out_start).seconds() / DCPTime(fade_out_end - fade_out_start).seconds();
	}
	if (time < fade_in_start || time > fade_out_end) {
		fade_factor = 0;
	}

	/* Render the subtitle at the top left-hand corner of image */

	Pango::FontDescription font (font_name);
	layout->set_font_description (font);
	layout->set_markup (marked_up (subtitles, target.height, fade_factor));

	context->scale (xscale, yscale);
	layout->update_from_cairo_context (context);

	/* Shuffle the subtitle over very slightly if it has a border so that the left-hand
	   side of the first character's border is not cut off.
	*/
	int const x_offset = subtitles.front().effect() == dcp::BORDER ? (target.width / 600.0) : 0;
	/* Move down a bit so that accents on capital letters can be seen */
	int const y_offset = target.height / 100.0;

	if (subtitles.front().effect() == dcp::SHADOW) {
		/* Drop-shadow effect */
		set_source_rgba (context, subtitles.front().effect_colour(), fade_factor);
		context->move_to (x_offset + 4, y_offset + 4);
		layout->add_to_cairo_context (context);
		context->fill ();
	}

	if (subtitles.front().effect() == dcp::BORDER) {
		/* Border effect; stroke the subtitle with a large (arbitrarily chosen) line width */
		set_source_rgba (context, subtitles.front().effect_colour(), fade_factor);
		context->set_line_width (subtitles.front().outline_width * target.width / 2048.0);
		context->set_line_join (Cairo::LINE_JOIN_ROUND);
		context->move_to (x_offset, y_offset);
		layout->add_to_cairo_context (context);
		context->stroke ();
	}

	/* The actual subtitle */

	context->set_line_width (0);
	context->move_to (x_offset, y_offset);
#ifdef DCPOMATIC_HAVE_SHOW_IN_CAIRO_CONTEXT
	layout->show_in_cairo_context (context);
#else
	pango_cairo_show_layout (context->cobj(), layout->gobj());
#endif

	int layout_width;
	int layout_height;
	layout->get_pixel_size (layout_width, layout_height);
	layout_width *= xscale;
	layout_height *= yscale;

	int x = 0;
	switch (subtitles.front().h_align ()) {
	case dcp::HALIGN_LEFT:
		/* h_position is distance between left of frame and left of subtitle */
		x = subtitles.front().h_position() * target.width;
		break;
	case dcp::HALIGN_CENTER:
		/* h_position is distance between centre of frame and centre of subtitle */
		x = (0.5 + subtitles.front().h_position()) * target.width - layout_width / 2;
		break;
	case dcp::HALIGN_RIGHT:
		/* h_position is distance between right of frame and right of subtitle */
		x = (1.0 - subtitles.front().h_position()) * target.width - layout_width;
		break;
	}

	int y = 0;
	switch (subtitles.front().v_align ()) {
	case dcp::VALIGN_TOP:
		/* SMPTE says that v_position is the distance between top
		   of frame and top of subtitle, but this doesn't always seem to be
		   the case in practice; Gunnar Ásgeirsson's Dolby server appears
		   to put VALIGN_TOP subs with v_position as the distance between top
		   of frame and bottom of subtitle.
		*/
		y = subtitles.front().v_position() * target.height - layout_height;
		break;
	case dcp::VALIGN_CENTER:
		/* v_position is distance between centre of frame and centre of subtitle */
		y = (0.5 + subtitles.front().v_position()) * target.height - layout_height / 2;
		break;
	case dcp::VALIGN_BOTTOM:
		/* v_position is distance between bottom of frame and bottom of subtitle */
		y = (1.0 - subtitles.front().v_position()) * target.height - layout_height;
		break;
	}

	return PositionImage (image, Position<int> (max (0, x), max (0, y)));
}

/** @param time Time of the frame that these subtitles are going on.
 *  @param frame_rate DCP frame rate.
 */
list<PositionImage>
render_text (list<PlainText> subtitles, list<shared_ptr<Font> > fonts, dcp::Size target, DCPTime time, int frame_rate)
{
	list<PlainText> pending;
	list<PositionImage> images;

	BOOST_FOREACH (PlainText const & i, subtitles) {
		if (!pending.empty() && fabs (i.v_position() - pending.back().v_position()) > 1e-4) {
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
