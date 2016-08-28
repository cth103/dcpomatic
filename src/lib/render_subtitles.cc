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

#include "render_subtitles.h"
#include "types.h"
#include "image.h"
#include "cross.h"
#include "font.h"
#include "dcpomatic_assert.h"
#include <fontconfig/fontconfig.h>
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
static list<pair<FontFiles, string> > fc_config_fonts;

string
marked_up (list<SubtitleString> subtitles)
{
	string out;
	bool italic = false;
	bool bold = false;
	bool underline = false;
	BOOST_FOREACH (SubtitleString const & i, subtitles) {

		if (i.italic() && !italic) {
			out += "<i>";
		}
		if (i.bold() && !bold) {
			out += "<b>";
		}
		if (i.underline() && !underline) {
			out += "<u>";
		}
		if (!i.underline() && underline) {
			out += "</u>";
		}
		if (!i.bold() && bold) {
			out += "</b>";
		}
		if (!i.italic() && italic) {
			out += "</i>";
		}

		italic = i.italic ();
		bold = i.bold ();
		underline = i.underline ();

		out += i.text ();
	}

	if (underline) {
		out += "</u>";
	}
	if (bold) {
		out += "</b>";
	}
	if (italic) {
		out += "</i>";
	}

	return out;
}

/** @param subtitles A list of subtitles that are all on the same line,
 *  at the same time and with the same fade in/out.
 */
static PositionImage
render_line (list<SubtitleString> subtitles, list<shared_ptr<Font> > fonts, dcp::Size target, DCPTime time)
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

	/* Basic guess on height... */
	int height = subtitles.front().size() * target.height / (11 * 72);
	/* ...scaled... */
	height *= yscale;
	/* ...and add a bit more for luck */
	height += target.height / 11;

	shared_ptr<Image> image (new Image (AV_PIX_FMT_RGBA, dcp::Size (target.width, height), false));
	image->make_black ();

#ifdef DCPOMATIC_HAVE_FORMAT_STRIDE_FOR_WIDTH
	Cairo::RefPtr<Cairo::ImageSurface> surface = Cairo::ImageSurface::create (
		image->data()[0],
		Cairo::FORMAT_ARGB32,
		image->size().width,
		image->size().height,
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

	/* Render the subtitle at the top left-hand corner of image */

	Pango::FontDescription font (font_name);
	font.set_absolute_size (subtitles.front().size_in_pixels (target.height) * PANGO_SCALE);
	layout->set_font_description (font);
	layout->set_markup (marked_up (subtitles));

	/* Compute fade factor */
	float fade_factor = 1;

	DCPTime const fade_in_start = DCPTime::from_seconds (subtitles.front().in().as_seconds ());
	DCPTime const fade_in_end = fade_in_start + DCPTime::from_seconds (subtitles.front().fade_up_time().as_seconds ());
	DCPTime const fade_out_end =  DCPTime::from_seconds (subtitles.front().out().as_seconds ());
	DCPTime const fade_out_start = fade_out_end - DCPTime::from_seconds (subtitles.front().fade_down_time().as_seconds ());
	if (fade_in_start <= time && time <= fade_in_end && fade_in_start != fade_in_end) {
		fade_factor = DCPTime(time - fade_in_start).seconds() / DCPTime(fade_in_end - fade_in_start).seconds();
	} else if (fade_out_start <= time && time <= fade_out_end && fade_out_start != fade_out_end) {
		fade_factor = 1 - DCPTime(time - fade_out_start).seconds() / DCPTime(fade_out_end - fade_out_start).seconds();
	} else if (time < fade_in_start || time > fade_out_end) {
		fade_factor = 0;
	}

	context->scale (xscale, yscale);
	layout->update_from_cairo_context (context);

	/* Shuffle the subtitle over very slightly if it has a border so that the left-hand
	   side of the first character's border is not cut off.
	*/
	int const x_offset = subtitles.front().effect() == dcp::BORDER ? (target.width / 600.0) : 0;

	if (subtitles.front().effect() == dcp::SHADOW) {
		/* Drop-shadow effect */
		dcp::Colour const ec = subtitles.front().effect_colour ();
		context->set_source_rgba (float(ec.r) / 255, float(ec.g) / 255, float(ec.b) / 255, fade_factor);
		context->move_to (x_offset + 4, 4);
		layout->add_to_cairo_context (context);
		context->fill ();
	}

	if (subtitles.front().effect() == dcp::BORDER) {
		/* Border effect; stroke the subtitle with a large (arbitrarily chosen) line width */
		dcp::Colour ec = subtitles.front().effect_colour ();
		context->set_source_rgba (float(ec.r) / 255, float(ec.g) / 255, float(ec.b) / 255, fade_factor);
		context->set_line_width (subtitles.front().outline_width * target.width / 2048.0);
		context->set_line_join (Cairo::LINE_JOIN_ROUND);
		context->move_to (x_offset, 0);
		layout->add_to_cairo_context (context);
		context->stroke ();
	}

	/* The actual subtitle */

	dcp::Colour const c = subtitles.front().colour ();
	context->set_source_rgba (float(c.r) / 255, float(c.g) / 255, float(c.b) / 255, fade_factor);
	context->set_line_width (0);
	context->move_to (x_offset, 0);
	layout->add_to_cairo_context (context);
	context->fill ();

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

/** @param time Time of the frame that these subtitles are going on */
list<PositionImage>
render_subtitles (list<SubtitleString> subtitles, list<shared_ptr<Font> > fonts, dcp::Size target, DCPTime time)
{
	list<SubtitleString> pending;
	list<PositionImage> images;

	BOOST_FOREACH (SubtitleString const & i, subtitles) {
		if (!pending.empty() && fabs (i.v_position() - pending.back().v_position()) > 1e-4) {
			images.push_back (render_line (pending, fonts, target, time));
			pending.clear ();
		}
		pending.push_back (i);
	}

	if (!pending.empty ()) {
		images.push_back (render_line (pending, fonts, target, time));
	}

	return images;
}
