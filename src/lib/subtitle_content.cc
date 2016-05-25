/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#include "subtitle_content.h"
#include "util.h"
#include "exceptions.h"
#include "safe_stringstream.h"
#include "font.h"
#include "raw_convert.h"
#include "content.h"
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

int const SubtitleContentProperty::X_OFFSET = 500;
int const SubtitleContentProperty::Y_OFFSET = 501;
int const SubtitleContentProperty::X_SCALE = 502;
int const SubtitleContentProperty::Y_SCALE = 503;
int const SubtitleContentProperty::USE = 504;
int const SubtitleContentProperty::BURN = 505;
int const SubtitleContentProperty::LANGUAGE = 506;
int const SubtitleContentProperty::FONTS = 507;
int const SubtitleContentProperty::COLOUR = 508;
int const SubtitleContentProperty::OUTLINE = 509;
int const SubtitleContentProperty::OUTLINE_COLOUR = 510;

SubtitleContent::SubtitleContent (Content* parent)
	: ContentPart (parent)
	, _use (false)
	, _burn (false)
	, _x_offset (0)
	, _y_offset (0)
	, _x_scale (1)
	, _y_scale (1)
	, _colour (255, 255, 255)
	, _outline (false)
	, _outline_colour (0, 0, 0)
{

}

shared_ptr<SubtitleContent>
SubtitleContent::from_xml (Content* parent, cxml::ConstNodePtr node, int version)
{
	if (!node->optional_number_child<double>("SubtitleXOffset") && !node->optional_number_child<double>("SubtitleOffset")) {
		return shared_ptr<SubtitleContent> ();
	}

	return shared_ptr<SubtitleContent> (new SubtitleContent (parent, node, version));
}

SubtitleContent::SubtitleContent (Content* parent, cxml::ConstNodePtr node, int version)
	: ContentPart (parent)
	, _use (false)
	, _burn (false)
	, _x_offset (0)
	, _y_offset (0)
	, _x_scale (1)
	, _y_scale (1)
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
{
	if (version >= 32) {
		_use = node->bool_child ("UseSubtitles");
		_burn = node->bool_child ("BurnSubtitles");
	}

	if (version >= 7) {
		_x_offset = node->number_child<double> ("SubtitleXOffset");
		_y_offset = node->number_child<double> ("SubtitleYOffset");
	} else {
		_y_offset = node->number_child<double> ("SubtitleOffset");
	}

	if (version >= 10) {
		_x_scale = node->number_child<double> ("SubtitleXScale");
		_y_scale = node->number_child<double> ("SubtitleYScale");
	} else {
		_x_scale = _y_scale = node->number_child<double> ("SubtitleScale");
	}

	_language = node->optional_string_child ("SubtitleLanguage").get_value_or ("");

	list<cxml::NodePtr> fonts = node->node_children ("Font");
	for (list<cxml::NodePtr>::const_iterator i = fonts.begin(); i != fonts.end(); ++i) {
		_fonts.push_back (shared_ptr<Font> (new Font (*i)));
	}

	connect_to_fonts ();
}

SubtitleContent::SubtitleContent (Content* parent, vector<shared_ptr<Content> > c)
	: ContentPart (parent)
{
	shared_ptr<SubtitleContent> ref = c[0]->subtitle;
	DCPOMATIC_ASSERT (ref);
	list<shared_ptr<Font> > ref_fonts = ref->fonts ();

	for (size_t i = 1; i < c.size(); ++i) {

		if (c[i]->subtitle->use() != ref->use()) {
			throw JoinError (_("Content to be joined must have the same 'use subtitles' setting."));
		}

		if (c[i]->subtitle->burn() != ref->burn()) {
			throw JoinError (_("Content to be joined must have the same 'burn subtitles' setting."));
		}

		if (c[i]->subtitle->x_offset() != ref->x_offset()) {
			throw JoinError (_("Content to be joined must have the same subtitle X offset."));
		}

		if (c[i]->subtitle->y_offset() != ref->y_offset()) {
			throw JoinError (_("Content to be joined must have the same subtitle Y offset."));
		}

		if (c[i]->subtitle->x_scale() != ref->x_scale()) {
			throw JoinError (_("Content to be joined must have the same subtitle X scale."));
		}

		if (c[i]->subtitle->y_scale() != ref->y_scale()) {
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

	_use = ref->use ();
	_burn = ref->burn ();
	_x_offset = ref->x_offset ();
	_y_offset = ref->y_offset ();
	_x_scale = ref->x_scale ();
	_y_scale = ref->y_scale ();
	_language = ref->language ();
	_fonts = ref_fonts;

	connect_to_fonts ();
}

/** _mutex must not be held on entry */
void
SubtitleContent::as_xml (xmlpp::Node* root) const
{
	boost::mutex::scoped_lock lm (_mutex);

	root->add_child("UseSubtitles")->add_child_text (raw_convert<string> (_use));
	root->add_child("BurnSubtitles")->add_child_text (raw_convert<string> (_burn));
	root->add_child("SubtitleXOffset")->add_child_text (raw_convert<string> (_x_offset));
	root->add_child("SubtitleYOffset")->add_child_text (raw_convert<string> (_y_offset));
	root->add_child("SubtitleXScale")->add_child_text (raw_convert<string> (_x_scale));
	root->add_child("SubtitleYScale")->add_child_text (raw_convert<string> (_y_scale));
	root->add_child("SubtitleLanguage")->add_child_text (_language);
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
	s << raw_convert<string> (x_scale())
	  << "_" << raw_convert<string> (y_scale())
	  << "_" << raw_convert<string> (x_offset())
	  << "_" << raw_convert<string> (y_offset());

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
	maybe_set (_colour, colour, SubtitleContentProperty::COLOUR);
}

void
SubtitleContent::set_outline (bool o)
{
	maybe_set (_outline, o, SubtitleContentProperty::OUTLINE);
}

void
SubtitleContent::set_outline_colour (dcp::Colour colour)
{
	maybe_set (_outline_colour, colour, SubtitleContentProperty::OUTLINE_COLOUR);
}

void
SubtitleContent::set_use (bool u)
{
	maybe_set (_use, u, SubtitleContentProperty::USE);
}

void
SubtitleContent::set_burn (bool b)
{
	maybe_set (_burn, b, SubtitleContentProperty::BURN);
}

void
SubtitleContent::set_x_offset (double o)
{
	maybe_set (_x_offset, o, SubtitleContentProperty::X_OFFSET);
}

void
SubtitleContent::set_y_offset (double o)
{
	maybe_set (_y_offset, o, SubtitleContentProperty::Y_OFFSET);
}

void
SubtitleContent::set_x_scale (double s)
{
	maybe_set (_x_scale, s, SubtitleContentProperty::X_SCALE);
}

void
SubtitleContent::set_y_scale (double s)
{
	maybe_set (_y_scale, s, SubtitleContentProperty::Y_SCALE);
}

void
SubtitleContent::set_language (string language)
{
	maybe_set (_language, language, SubtitleContentProperty::LANGUAGE);
}
