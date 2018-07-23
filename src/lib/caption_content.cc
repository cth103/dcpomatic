/*
    Copyright (C) 2013-2018 Carl Hetherington <cth@carlh.net>

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

#include "caption_content.h"
#include "util.h"
#include "exceptions.h"
#include "font.h"
#include "content.h"
#include <dcp/raw_convert.h>
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
using boost::optional;
using dcp::raw_convert;

int const CaptionContentProperty::X_OFFSET = 500;
int const CaptionContentProperty::Y_OFFSET = 501;
int const CaptionContentProperty::X_SCALE = 502;
int const CaptionContentProperty::Y_SCALE = 503;
int const CaptionContentProperty::USE = 504;
int const CaptionContentProperty::BURN = 505;
int const CaptionContentProperty::LANGUAGE = 506;
int const CaptionContentProperty::FONTS = 507;
int const CaptionContentProperty::COLOUR = 508;
int const CaptionContentProperty::EFFECT = 509;
int const CaptionContentProperty::EFFECT_COLOUR = 510;
int const CaptionContentProperty::LINE_SPACING = 511;
int const CaptionContentProperty::FADE_IN = 512;
int const CaptionContentProperty::FADE_OUT = 513;
int const CaptionContentProperty::OUTLINE_WIDTH = 514;
int const CaptionContentProperty::TYPE = 515;

CaptionContent::CaptionContent (Content* parent, CaptionType original_type)
	: ContentPart (parent)
	, _use (false)
	, _burn (false)
	, _x_offset (0)
	, _y_offset (0)
	, _x_scale (1)
	, _y_scale (1)
	, _line_spacing (1)
	, _outline_width (2)
	, _type (original_type)
	, _original_type (original_type)
{

}

/** @return CaptionContents from node or <Caption> nodes under node (according to version).
 *  The list could be empty if no CaptionContents are found.
 */
list<shared_ptr<CaptionContent> >
CaptionContent::from_xml (Content* parent, cxml::ConstNodePtr node, int version)
{
	if (version < 34) {
		/* With old metadata FFmpeg content has the subtitle-related tags even with no
		   subtitle streams, so check for that.
		*/
		if (node->string_child("Type") == "FFmpeg" && node->node_children("SubtitleStream").empty()) {
			return list<shared_ptr<CaptionContent> >();
		}

		/* Otherwise we can drop through to the newer logic */
	}

	if (version < 37) {
		if (!node->optional_number_child<double>("SubtitleXOffset") && !node->optional_number_child<double>("SubtitleOffset")) {
			return list<shared_ptr<CaptionContent> >();
		}
		list<shared_ptr<CaptionContent> > c;
		c.push_back (shared_ptr<CaptionContent> (new CaptionContent (parent, node, version)));
		return c;
	}

	if (!node->optional_node_child("Caption")) {
		return list<shared_ptr<CaptionContent> >();
	}

	list<shared_ptr<CaptionContent> > c;
	BOOST_FOREACH (cxml::ConstNodePtr i, node->node_children("Caption")) {
		c.push_back (shared_ptr<CaptionContent> (new CaptionContent (parent, i, version)));
	}
	return c;
}

CaptionContent::CaptionContent (Content* parent, cxml::ConstNodePtr node, int version)
	: ContentPart (parent)
	, _use (false)
	, _burn (false)
	, _x_offset (0)
	, _y_offset (0)
	, _x_scale (1)
	, _y_scale (1)
	, _line_spacing (node->optional_number_child<double>("LineSpacing").get_value_or (1))
	, _outline_width (node->optional_number_child<int>("OutlineWidth").get_value_or (2))
	, _type (CAPTION_OPEN)
	, _original_type (CAPTION_OPEN)
{
	if (version >= 37) {
		_use = node->bool_child ("Use");
		_burn = node->bool_child ("Burn");
	} else if (version >= 32) {
		_use = node->bool_child ("UseSubtitles");
		_burn = node->bool_child ("BurnSubtitles");
	}

	if (version >= 37) {
		_x_offset = node->number_child<double> ("XOffset");
		_y_offset = node->number_child<double> ("YOffset");
	} else if (version >= 7) {
		_x_offset = node->number_child<double> ("SubtitleXOffset");
		_y_offset = node->number_child<double> ("SubtitleYOffset");
	} else {
		_y_offset = node->number_child<double> ("SubtitleOffset");
	}

	if (node->optional_bool_child("Outline").get_value_or(false)) {
		_effect = dcp::BORDER;
	} else if (node->optional_bool_child("Shadow").get_value_or(false)) {
		_effect = dcp::SHADOW;
	} else {
		_effect = dcp::NONE;
	}

	optional<string> effect = node->optional_string_child("Effect");
	if (effect) {
		if (*effect == "none") {
			_effect = dcp::NONE;
		} else if (*effect == "outline") {
			_effect = dcp::BORDER;
		} else if (*effect == "shadow") {
			_effect = dcp::SHADOW;
		}
	}

	if (version >= 37) {
		_x_scale = node->number_child<double> ("XScale");
		_y_scale = node->number_child<double> ("YScale");
	} else if (version >= 10) {
		_x_scale = node->number_child<double> ("SubtitleXScale");
		_y_scale = node->number_child<double> ("SubtitleYScale");
	} else {
		_x_scale = _y_scale = node->number_child<double> ("SubtitleScale");
	}

	optional<int> r = node->optional_number_child<int>("Red");
	optional<int> g = node->optional_number_child<int>("Green");
	optional<int> b = node->optional_number_child<int>("Blue");
	if (r && g && b) {
		_colour = dcp::Colour (*r, *g, *b);
	}

	if (version >= 36) {
		optional<int> er = node->optional_number_child<int>("EffectRed");
		optional<int> eg = node->optional_number_child<int>("EffectGreen");
		optional<int> eb = node->optional_number_child<int>("EffectBlue");
		if (er && eg && eb) {
			_effect_colour = dcp::Colour (*er, *eg, *eb);
		}
	} else {
		_effect_colour = dcp::Colour (
			node->optional_number_child<int>("OutlineRed").get_value_or(255),
			node->optional_number_child<int>("OutlineGreen").get_value_or(255),
			node->optional_number_child<int>("OutlineBlue").get_value_or(255)
			);
	}

	optional<Frame> fi;
	if (version >= 37) {
		fi = node->optional_number_child<Frame>("FadeIn");
	} else {
		fi = node->optional_number_child<Frame>("SubtitleFadeIn");
	}
	if (fi) {
		_fade_in = ContentTime (*fi);
	}

	optional<Frame> fo;
	if (version >= 37) {
		fo = node->optional_number_child<Frame>("FadeOut");
	} else {
		fo = node->optional_number_child<Frame>("SubtitleFadeOut");
	}
	if (fo) {
		_fade_out = ContentTime (*fo);
	}

	if (version >= 37) {
		_language = node->optional_string_child ("Language").get_value_or ("");
	} else {
		_language = node->optional_string_child ("SubtitleLanguage").get_value_or ("");
	}

	list<cxml::NodePtr> fonts = node->node_children ("Font");
	for (list<cxml::NodePtr>::const_iterator i = fonts.begin(); i != fonts.end(); ++i) {
		_fonts.push_back (shared_ptr<Font> (new Font (*i)));
	}

	connect_to_fonts ();

	_type = string_to_caption_type (node->optional_string_child("Type").get_value_or("open"));
	_original_type = string_to_caption_type (node->optional_string_child("OriginalType").get_value_or("open"));
}

CaptionContent::CaptionContent (Content* parent, vector<shared_ptr<Content> > c)
	: ContentPart (parent)
{
	/* This constructor is for join which is only supported for content types
	   that have a single caption, so we can use only_caption() here.
	*/
	shared_ptr<CaptionContent> ref = c[0]->only_caption();
	DCPOMATIC_ASSERT (ref);
	list<shared_ptr<Font> > ref_fonts = ref->fonts ();

	for (size_t i = 1; i < c.size(); ++i) {

		if (c[i]->only_caption()->use() != ref->use()) {
			throw JoinError (_("Content to be joined must have the same 'use subtitles' setting."));
		}

		if (c[i]->only_caption()->burn() != ref->burn()) {
			throw JoinError (_("Content to be joined must have the same 'burn subtitles' setting."));
		}

		if (c[i]->only_caption()->x_offset() != ref->x_offset()) {
			throw JoinError (_("Content to be joined must have the same subtitle X offset."));
		}

		if (c[i]->only_caption()->y_offset() != ref->y_offset()) {
			throw JoinError (_("Content to be joined must have the same subtitle Y offset."));
		}

		if (c[i]->only_caption()->x_scale() != ref->x_scale()) {
			throw JoinError (_("Content to be joined must have the same subtitle X scale."));
		}

		if (c[i]->only_caption()->y_scale() != ref->y_scale()) {
			throw JoinError (_("Content to be joined must have the same subtitle Y scale."));
		}

		if (c[i]->only_caption()->line_spacing() != ref->line_spacing()) {
			throw JoinError (_("Content to be joined must have the same subtitle line spacing."));
		}

		if ((c[i]->only_caption()->fade_in() != ref->fade_in()) || (c[i]->only_caption()->fade_out() != ref->fade_out())) {
			throw JoinError (_("Content to be joined must have the same subtitle fades."));
		}

		if ((c[i]->only_caption()->outline_width() != ref->outline_width())) {
			throw JoinError (_("Content to be joined must have the same outline width."));
		}

		list<shared_ptr<Font> > fonts = c[i]->only_caption()->fonts ();
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
	_line_spacing = ref->line_spacing ();
	_fade_in = ref->fade_in ();
	_fade_out = ref->fade_out ();
	_outline_width = ref->outline_width ();
	_type = ref->type ();
	_original_type = ref->original_type ();

	connect_to_fonts ();
}

/** _mutex must not be held on entry */
void
CaptionContent::as_xml (xmlpp::Node* root) const
{
	boost::mutex::scoped_lock lm (_mutex);

	xmlpp::Element* caption = root->add_child ("Caption");

	caption->add_child("Use")->add_child_text (_use ? "1" : "0");
	caption->add_child("Burn")->add_child_text (_burn ? "1" : "0");
	caption->add_child("XOffset")->add_child_text (raw_convert<string> (_x_offset));
	caption->add_child("YOffset")->add_child_text (raw_convert<string> (_y_offset));
	caption->add_child("XScale")->add_child_text (raw_convert<string> (_x_scale));
	caption->add_child("YScale")->add_child_text (raw_convert<string> (_y_scale));
	caption->add_child("Language")->add_child_text (_language);
	if (_colour) {
		caption->add_child("Red")->add_child_text (raw_convert<string> (_colour->r));
		caption->add_child("Green")->add_child_text (raw_convert<string> (_colour->g));
		caption->add_child("Blue")->add_child_text (raw_convert<string> (_colour->b));
	}
	if (_effect) {
		switch (*_effect) {
		case dcp::NONE:
			caption->add_child("Effect")->add_child_text("none");
			break;
		case dcp::BORDER:
			caption->add_child("Effect")->add_child_text("outline");
			break;
		case dcp::SHADOW:
			caption->add_child("Effect")->add_child_text("shadow");
			break;
		}
	}
	if (_effect_colour) {
		caption->add_child("EffectRed")->add_child_text (raw_convert<string> (_effect_colour->r));
		caption->add_child("EffectGreen")->add_child_text (raw_convert<string> (_effect_colour->g));
		caption->add_child("EffectBlue")->add_child_text (raw_convert<string> (_effect_colour->b));
	}
	caption->add_child("LineSpacing")->add_child_text (raw_convert<string> (_line_spacing));
	if (_fade_in) {
		caption->add_child("FadeIn")->add_child_text (raw_convert<string> (_fade_in->get()));
	}
	if (_fade_out) {
		caption->add_child("FadeOut")->add_child_text (raw_convert<string> (_fade_out->get()));
	}
	caption->add_child("OutlineWidth")->add_child_text (raw_convert<string> (_outline_width));

	for (list<shared_ptr<Font> >::const_iterator i = _fonts.begin(); i != _fonts.end(); ++i) {
		(*i)->as_xml (caption->add_child("Font"));
	}

	caption->add_child("Type")->add_child_text (caption_type_to_string(_type));
	caption->add_child("OriginalType")->add_child_text (caption_type_to_string(_original_type));
}

string
CaptionContent::identifier () const
{
	string s = raw_convert<string> (x_scale())
		+ "_" + raw_convert<string> (y_scale())
		+ "_" + raw_convert<string> (x_offset())
		+ "_" + raw_convert<string> (y_offset())
		+ "_" + raw_convert<string> (line_spacing())
		+ "_" + raw_convert<string> (fade_in().get_value_or(ContentTime()).get())
		+ "_" + raw_convert<string> (fade_out().get_value_or(ContentTime()).get())
		+ "_" + raw_convert<string> (outline_width())
		+ "_" + raw_convert<string> (colour().get_value_or(dcp::Colour(255, 255, 255)).to_argb_string())
		+ "_" + raw_convert<string> (dcp::effect_to_string(effect().get_value_or(dcp::NONE)))
		+ "_" + raw_convert<string> (effect_colour().get_value_or(dcp::Colour(0, 0, 0)).to_argb_string());

	/* XXX: I suppose really _fonts shouldn't be in here, since not all
	   types of subtitle content involve fonts.
	*/
	BOOST_FOREACH (shared_ptr<Font> f, _fonts) {
		for (int i = 0; i < FontFiles::VARIANTS; ++i) {
			s += "_" + f->file(static_cast<FontFiles::Variant>(i)).get_value_or("Default").string();
		}
	}

	/* The language is for metadata only, and doesn't affect
	   how this content looks.
	*/

	return s;
}

void
CaptionContent::add_font (shared_ptr<Font> font)
{
	_fonts.push_back (font);
	connect_to_fonts ();
}

void
CaptionContent::connect_to_fonts ()
{
	BOOST_FOREACH (boost::signals2::connection& i, _font_connections) {
		i.disconnect ();
	}

	_font_connections.clear ();

	BOOST_FOREACH (shared_ptr<Font> i, _fonts) {
		_font_connections.push_back (i->Changed.connect (boost::bind (&CaptionContent::font_changed, this)));
	}
}

void
CaptionContent::font_changed ()
{
	_parent->signal_changed (CaptionContentProperty::FONTS);
}

void
CaptionContent::set_colour (dcp::Colour colour)
{
	maybe_set (_colour, colour, CaptionContentProperty::COLOUR);
}

void
CaptionContent::unset_colour ()
{
	maybe_set (_colour, optional<dcp::Colour>(), CaptionContentProperty::COLOUR);
}

void
CaptionContent::set_effect (dcp::Effect e)
{
	maybe_set (_effect, e, CaptionContentProperty::EFFECT);
}

void
CaptionContent::unset_effect ()
{
	maybe_set (_effect, optional<dcp::Effect>(), CaptionContentProperty::EFFECT);
}

void
CaptionContent::set_effect_colour (dcp::Colour colour)
{
	maybe_set (_effect_colour, colour, CaptionContentProperty::EFFECT_COLOUR);
}

void
CaptionContent::unset_effect_colour ()
{
	maybe_set (_effect_colour, optional<dcp::Colour>(), CaptionContentProperty::EFFECT_COLOUR);
}

void
CaptionContent::set_use (bool u)
{
	maybe_set (_use, u, CaptionContentProperty::USE);
}

void
CaptionContent::set_burn (bool b)
{
	maybe_set (_burn, b, CaptionContentProperty::BURN);
}

void
CaptionContent::set_x_offset (double o)
{
	maybe_set (_x_offset, o, CaptionContentProperty::X_OFFSET);
}

void
CaptionContent::set_y_offset (double o)
{
	maybe_set (_y_offset, o, CaptionContentProperty::Y_OFFSET);
}

void
CaptionContent::set_x_scale (double s)
{
	maybe_set (_x_scale, s, CaptionContentProperty::X_SCALE);
}

void
CaptionContent::set_y_scale (double s)
{
	maybe_set (_y_scale, s, CaptionContentProperty::Y_SCALE);
}

void
CaptionContent::set_language (string language)
{
	maybe_set (_language, language, CaptionContentProperty::LANGUAGE);
}

void
CaptionContent::set_line_spacing (double s)
{
	maybe_set (_line_spacing, s, CaptionContentProperty::LINE_SPACING);
}

void
CaptionContent::set_fade_in (ContentTime t)
{
	maybe_set (_fade_in, t, CaptionContentProperty::FADE_IN);
}

void
CaptionContent::unset_fade_in ()
{
	maybe_set (_fade_in, optional<ContentTime>(), CaptionContentProperty::FADE_IN);
}

void
CaptionContent::set_fade_out (ContentTime t)
{
	maybe_set (_fade_out, t, CaptionContentProperty::FADE_OUT);
}

void
CaptionContent::unset_fade_out ()
{
	maybe_set (_fade_out, optional<ContentTime>(), CaptionContentProperty::FADE_OUT);
}

void
CaptionContent::set_type (CaptionType type)
{
	maybe_set (_type, type, CaptionContentProperty::TYPE);
}

void
CaptionContent::set_outline_width (int w)
{
	maybe_set (_outline_width, w, CaptionContentProperty::OUTLINE_WIDTH);
}

void
CaptionContent::take_settings_from (shared_ptr<const CaptionContent> c)
{
	set_use (c->_use);
	set_burn (c->_burn);
	set_x_offset (c->_x_offset);
	set_y_offset (c->_y_offset);
	set_x_scale (c->_x_scale);
	set_y_scale (c->_y_scale);
	maybe_set (_fonts, c->_fonts, CaptionContentProperty::FONTS);
	if (c->_colour) {
		set_colour (*c->_colour);
	} else {
		unset_colour ();
	}
	if (c->_effect) {
		set_effect (*c->_effect);
	}
	if (c->_effect_colour) {
		set_effect_colour (*c->_effect_colour);
	} else {
		unset_effect_colour ();
	}
	set_line_spacing (c->_line_spacing);
	if (c->_fade_in) {
		set_fade_in (*c->_fade_in);
	}
	if (c->_fade_out) {
		set_fade_out (*c->_fade_out);
	}
	set_outline_width (c->_outline_width);
}
