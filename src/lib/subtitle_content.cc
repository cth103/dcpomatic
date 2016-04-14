/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#include "subtitle_content.h"
#include "util.h"
#include "exceptions.h"
#include "safe_stringstream.h"
#include "font.h"
#include "raw_convert.h"
#include "content.h"
#include "film.h"
#include <libcxml/cxml.h>
#include <libxml++/libxml++.h>
#include <boost/foreach.hpp>
#include <iostream>

#include "i18n.h"

using std::string;
using std::vector;
using std::cout;
using std::list;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;

int const SubtitleContentProperty::SUBTITLE_X_OFFSET = 500;
int const SubtitleContentProperty::SUBTITLE_Y_OFFSET = 501;
int const SubtitleContentProperty::SUBTITLE_X_SCALE = 502;
int const SubtitleContentProperty::SUBTITLE_Y_SCALE = 503;
int const SubtitleContentProperty::USE_SUBTITLES = 504;
int const SubtitleContentProperty::BURN_SUBTITLES = 505;
int const SubtitleContentProperty::SUBTITLE_LANGUAGE = 506;
int const SubtitleContentProperty::FONTS = 507;
int const SubtitleContentProperty::SUBTITLE_VIDEO_FRAME_RATE = 508;
int const SubtitleContentProperty::SUBTITLE_COLOUR = 509;
int const SubtitleContentProperty::SUBTITLE_OUTLINE = 510;
int const SubtitleContentProperty::SUBTITLE_OUTLINE_COLOUR = 511;

SubtitleContent::SubtitleContent (Content* parent, shared_ptr<const Film> film)
	: ContentPart (parent, film)
	, _use_subtitles (false)
	, _burn_subtitles (false)
	, _subtitle_x_offset (0)
	, _subtitle_y_offset (0)
	, _subtitle_x_scale (1)
	, _subtitle_y_scale (1)
	, _colour (255, 255, 255)
	, _outline (false)
	, _outline_colour (0, 0, 0)
{

}

SubtitleContent::SubtitleContent (Content* parent, shared_ptr<const Film> film, cxml::ConstNodePtr node, int version)
	: ContentPart (parent, film)
	, _use_subtitles (false)
	, _burn_subtitles (false)
	, _subtitle_x_offset (0)
	, _subtitle_y_offset (0)
	, _subtitle_x_scale (1)
	, _subtitle_y_scale (1)
	, _colour (
		node->optional_number_child<int>("Red").get_value_or(255),
		node->optional_number_child<int>("Green").get_value_or(255),
		node->optional_number_child<int>("Blue").get_value_or(255)
		)
	, _outline (node->optional_bool_child("Outline").get_value_or(false))
	, _outline_colour (
		node->optional_number_child<int>("OutlineRed").get_value_or(255),
		node->optional_number_child<int>("OutlineGreen").get_value_or(255),
		node->optional_number_child<int>("OutlineBlue").get_value_or(255)
		)
	, _frame_rate (node->optional_number_child<double>("SubtitleFrameRate"))
{
	if (version >= 32) {
		_use_subtitles = node->bool_child ("UseSubtitles");
		_burn_subtitles = node->bool_child ("BurnSubtitles");
	}

	if (version >= 7) {
		_subtitle_x_offset = node->number_child<double> ("SubtitleXOffset");
		_subtitle_y_offset = node->number_child<double> ("SubtitleYOffset");
	} else {
		_subtitle_y_offset = node->number_child<double> ("SubtitleOffset");
	}

	if (version >= 10) {
		_subtitle_x_scale = node->number_child<double> ("SubtitleXScale");
		_subtitle_y_scale = node->number_child<double> ("SubtitleYScale");
	} else {
		_subtitle_x_scale = _subtitle_y_scale = node->number_child<double> ("SubtitleScale");
	}

	_subtitle_language = node->optional_string_child ("SubtitleLanguage").get_value_or ("");

	list<cxml::NodePtr> fonts = node->node_children ("Font");
	for (list<cxml::NodePtr>::const_iterator i = fonts.begin(); i != fonts.end(); ++i) {
		_fonts.push_back (shared_ptr<Font> (new Font (*i)));
	}

	connect_to_fonts ();
}

SubtitleContent::SubtitleContent (Content* parent, shared_ptr<const Film> film, vector<shared_ptr<Content> > c)
	: ContentPart (parent, film)
{
	shared_ptr<SubtitleContent> ref = c[0]->subtitle;
	DCPOMATIC_ASSERT (ref);
	list<shared_ptr<Font> > ref_fonts = ref->fonts ();

	for (size_t i = 1; i < c.size(); ++i) {

		if (c[i]->subtitle->use_subtitles() != ref->use_subtitles()) {
			throw JoinError (_("Content to be joined must have the same 'use subtitles' setting."));
		}

		if (c[i]->subtitle->burn_subtitles() != ref->burn_subtitles()) {
			throw JoinError (_("Content to be joined must have the same 'burn subtitles' setting."));
		}

		if (c[i]->subtitle->subtitle_x_offset() != ref->subtitle_x_offset()) {
			throw JoinError (_("Content to be joined must have the same subtitle X offset."));
		}

		if (c[i]->subtitle->subtitle_y_offset() != ref->subtitle_y_offset()) {
			throw JoinError (_("Content to be joined must have the same subtitle Y offset."));
		}

		if (c[i]->subtitle->subtitle_x_scale() != ref->subtitle_x_scale()) {
			throw JoinError (_("Content to be joined must have the same subtitle X scale."));
		}

		if (c[i]->subtitle->subtitle_y_scale() != ref->subtitle_y_scale()) {
			throw JoinError (_("Content to be joined must have the same subtitle Y scale."));
		}

		list<shared_ptr<Font> > fonts = c[i]->subtitle->fonts ();
		if (fonts.size() != ref_fonts.size()) {
			throw JoinError (_("Content to be joined must use the same fonts."));
		}

		list<shared_ptr<Font> >::const_iterator j = ref_fonts.begin ();
		list<shared_ptr<Font> >::const_iterator k = fonts.begin ();

		while (j != ref_fonts.end ()) {
			if (**j != **k) {
				throw JoinError (_("Content to be joined must use the same fonts."));
			}
			++j;
			++k;
		}
	}

	_use_subtitles = ref->use_subtitles ();
	_burn_subtitles = ref->burn_subtitles ();
	_subtitle_x_offset = ref->subtitle_x_offset ();
	_subtitle_y_offset = ref->subtitle_y_offset ();
	_subtitle_x_scale = ref->subtitle_x_scale ();
	_subtitle_y_scale = ref->subtitle_y_scale ();
	_subtitle_language = ref->subtitle_language ();
	_fonts = ref_fonts;

	connect_to_fonts ();
}

/** _mutex must not be held on entry */
void
SubtitleContent::as_xml (xmlpp::Node* root) const
{
	boost::mutex::scoped_lock lm (_mutex);

	root->add_child("UseSubtitles")->add_child_text (raw_convert<string> (_use_subtitles));
	root->add_child("BurnSubtitles")->add_child_text (raw_convert<string> (_burn_subtitles));
	root->add_child("SubtitleXOffset")->add_child_text (raw_convert<string> (_subtitle_x_offset));
	root->add_child("SubtitleYOffset")->add_child_text (raw_convert<string> (_subtitle_y_offset));
	root->add_child("SubtitleXScale")->add_child_text (raw_convert<string> (_subtitle_x_scale));
	root->add_child("SubtitleYScale")->add_child_text (raw_convert<string> (_subtitle_y_scale));
	root->add_child("SubtitleLanguage")->add_child_text (_subtitle_language);
	root->add_child("Red")->add_child_text (raw_convert<string> (_colour.r));
	root->add_child("Green")->add_child_text (raw_convert<string> (_colour.g));
	root->add_child("Blue")->add_child_text (raw_convert<string> (_colour.b));
	root->add_child("Outline")->add_child_text (raw_convert<string> (_outline));
	root->add_child("OutlineRed")->add_child_text (raw_convert<string> (_outline_colour.r));
	root->add_child("OutlineGreen")->add_child_text (raw_convert<string> (_outline_colour.g));
	root->add_child("OutlineBlue")->add_child_text (raw_convert<string> (_outline_colour.b));

	for (list<shared_ptr<Font> >::const_iterator i = _fonts.begin(); i != _fonts.end(); ++i) {
		(*i)->as_xml (root->add_child("Font"));
	}
}

string
SubtitleContent::identifier () const
{
	SafeStringStream s;
	s << raw_convert<string> (subtitle_x_scale())
	  << "_" << raw_convert<string> (subtitle_y_scale())
	  << "_" << raw_convert<string> (subtitle_x_offset())
	  << "_" << raw_convert<string> (subtitle_y_offset());

	/* XXX: I suppose really _fonts shouldn't be in here, since not all
	   types of subtitle content involve fonts.
	*/
	BOOST_FOREACH (shared_ptr<Font> f, _fonts) {
		for (int i = 0; i < FontFiles::VARIANTS; ++i) {
			s << "_" << f->file(static_cast<FontFiles::Variant>(i)).get_value_or ("Default");
		}
	}

	/* The language is for metadata only, and doesn't affect
	   how this content looks.
	*/

	return s.str ();
}

void
SubtitleContent::add_font (shared_ptr<Font> font)
{
	_fonts.push_back (font);
	connect_to_fonts ();
}

void
SubtitleContent::connect_to_fonts ()
{
	BOOST_FOREACH (boost::signals2::connection& i, _font_connections) {
		i.disconnect ();
	}

	_font_connections.clear ();

	BOOST_FOREACH (shared_ptr<Font> i, _fonts) {
		_font_connections.push_back (i->Changed.connect (boost::bind (&SubtitleContent::font_changed, this)));
	}
}

void
SubtitleContent::font_changed ()
{
	_parent->signal_changed (SubtitleContentProperty::FONTS);
}

void
SubtitleContent::set_colour (dcp::Colour colour)
{
	maybe_set (_colour, colour, SubtitleContentProperty::SUBTITLE_COLOUR);
}

void
SubtitleContent::set_outline (bool o)
{
	maybe_set (_outline, o, SubtitleContentProperty::SUBTITLE_OUTLINE);
}

void
SubtitleContent::set_outline_colour (dcp::Colour colour)
{
	maybe_set (_outline_colour, colour, SubtitleContentProperty::SUBTITLE_OUTLINE_COLOUR);
}

void
SubtitleContent::set_use_subtitles (bool u)
{
	maybe_set (_use_subtitles, u, SubtitleContentProperty::USE_SUBTITLES);
}

void
SubtitleContent::set_burn_subtitles (bool b)
{
	maybe_set (_burn_subtitles, b, SubtitleContentProperty::BURN_SUBTITLES);
}

void
SubtitleContent::set_subtitle_x_offset (double o)
{
	maybe_set (_subtitle_x_offset, o, SubtitleContentProperty::SUBTITLE_X_OFFSET);
}

void
SubtitleContent::set_subtitle_y_offset (double o)
{
	maybe_set (_subtitle_y_offset, o, SubtitleContentProperty::SUBTITLE_Y_OFFSET);
}

void
SubtitleContent::set_subtitle_x_scale (double s)
{
	maybe_set (_subtitle_x_scale, s, SubtitleContentProperty::SUBTITLE_X_SCALE);
}

void
SubtitleContent::set_subtitle_y_scale (double s)
{
	maybe_set (_subtitle_y_scale, s, SubtitleContentProperty::SUBTITLE_Y_SCALE);
}

void
SubtitleContent::set_subtitle_language (string language)
{
	maybe_set (_subtitle_language, language, SubtitleContentProperty::SUBTITLE_LANGUAGE);
}

void
SubtitleContent::set_subtitle_video_frame_rate (double r)
{
	maybe_set (_frame_rate, r, SubtitleContentProperty::SUBTITLE_VIDEO_FRAME_RATE);
}

double
SubtitleContent::subtitle_video_frame_rate () const
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_frame_rate) {
			return _frame_rate.get ();
		}
	}

	/* No frame rate specified, so assume this content has been
	   prepared for any concurrent video content.
	*/
	shared_ptr<const Film> film = _film.lock ();
	DCPOMATIC_ASSERT (film);
	return film->active_frame_rate_change(_parent->position()).source;
}
