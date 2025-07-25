/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#include "content.h"
#include "exceptions.h"
#include "font.h"
#include "text_content.h"
#include "util.h"
#include "variant.h"
#include <libcxml/cxml.h>
#include <libxml++/libxml++.h>
#include <fmt/format.h>
#include <iostream>

#include "i18n.h"


using std::cout;
using std::dynamic_pointer_cast;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
using namespace dcpomatic;


TextContent::TextContent(Content* parent, TextType type, TextType original_type)
	: ContentPart(parent)
	, _use(false)
	, _burn(false)
	, _x_offset(0)
	, _y_offset(0)
	, _x_scale(1)
	, _y_scale(1)
	, _line_spacing(1)
	, _outline_width(4)
	, _type(type)
	, _original_type(original_type)
{

}

/** @return TextContents from node or <Text> nodes under node (according to version).
 *  The vector could be empty if no TextContents are found.
 */
vector<shared_ptr<TextContent>>
TextContent::from_xml(Content* parent, cxml::ConstNodePtr node, int version, list<string>& notes)
{
	if (version < 34) {
		/* With old metadata FFmpeg content has the subtitle-related tags even with no
		   subtitle streams, so check for that.
		*/
		if (node->string_child("Type") == "FFmpeg" && node->node_children("SubtitleStream").empty()) {
			return {};
		}

		/* Otherwise we can drop through to the newer logic */
	}

	if (version < 37) {
		if (!node->optional_number_child<double>("SubtitleXOffset") && !node->optional_number_child<double>("SubtitleOffset")) {
			return {};
		}
		return { make_shared<TextContent>(parent, node, version, notes) };
	}

	vector<shared_ptr<TextContent>> content;
	for (auto i: node->node_children("Text")) {
		content.push_back(make_shared<TextContent>(parent, i, version, notes));
	}

	return content;
}


TextContent::TextContent(Content* parent, cxml::ConstNodePtr node, int version, list<string>& notes)
	: ContentPart(parent)
	, _use(false)
	, _burn(false)
	, _x_offset(0)
	, _y_offset(0)
	, _x_scale(1)
	, _y_scale(1)
	, _line_spacing(node->optional_number_child<double>("LineSpacing").get_value_or(1))
	, _outline_width(node->optional_number_child<int>("OutlineWidth").get_value_or(4))
	, _type(TextType::OPEN_SUBTITLE)
	, _original_type(TextType::OPEN_SUBTITLE)
{
	if (version >= 37) {
		_use = node->bool_child("Use");
		_burn = node->bool_child("Burn");
	} else if (version >= 32) {
		_use = node->bool_child("UseSubtitles");
		_burn = node->bool_child("BurnSubtitles");
	}

	if (version >= 37) {
		_x_offset = node->number_child<double>("XOffset");
		_y_offset = node->number_child<double>("YOffset");
	} else if (version >= 7) {
		_x_offset = node->number_child<double>("SubtitleXOffset");
		_y_offset = node->number_child<double>("SubtitleYOffset");
	} else {
		_y_offset = node->number_child<double>("SubtitleOffset");
	}

	if (node->optional_bool_child("Outline").get_value_or(false)) {
		_effect = dcp::Effect::BORDER;
	} else if (node->optional_bool_child("Shadow").get_value_or(false)) {
		_effect = dcp::Effect::SHADOW;
	}

	auto effect = node->optional_string_child("Effect");
	if (effect) {
		if (*effect == "none") {
			_effect = dcp::Effect::NONE;
		} else if (*effect == "outline") {
			_effect = dcp::Effect::BORDER;
		} else if (*effect == "shadow") {
			_effect = dcp::Effect::SHADOW;
		}
	}

	if (version >= 37) {
		_x_scale = node->number_child<double>("XScale");
		_y_scale = node->number_child<double>("YScale");
	} else if (version >= 10) {
		_x_scale = node->number_child<double>("SubtitleXScale");
		_y_scale = node->number_child<double>("SubtitleYScale");
	} else {
		_x_scale = _y_scale = node->number_child<double>("SubtitleScale");
	}

	auto r = node->optional_number_child<int>("Red");
	auto g = node->optional_number_child<int>("Green");
	auto b = node->optional_number_child<int>("Blue");
	if (r && g && b) {
		_colour = dcp::Colour(*r, *g, *b);
	}

	if (version >= 36) {
		auto er = node->optional_number_child<int>("EffectRed");
		auto eg = node->optional_number_child<int>("EffectGreen");
		auto eb = node->optional_number_child<int>("EffectBlue");
		if (er && eg && eb) {
			_effect_colour = dcp::Colour(*er, *eg, *eb);
		}
	} else {
		_effect_colour = dcp::Colour(
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
		_fade_in = ContentTime(*fi);
	}

	optional<Frame> fo;
	if (version >= 37) {
		fo = node->optional_number_child<Frame>("FadeOut");
	} else {
		fo = node->optional_number_child<Frame>("SubtitleFadeOut");
	}
	if (fo) {
		_fade_out = ContentTime(*fo);
	}

	for (auto i: node->node_children("Font")) {
		_fonts.push_back(make_shared<Font>(i));
	}

	connect_to_fonts();

	if (version >= 37) {
		_type = string_to_text_type(node->optional_string_child("Type").get_value_or("open"));
		if (node->optional_string_child("OriginalType")) {
			_original_type = string_to_text_type(node->optional_string_child("OriginalType").get());
		}
	}

	auto dt = node->optional_node_child("DCPTrack");
	if (dt) {
		_dcp_track = DCPTextTrack(dt);
	}

	auto lang = node->optional_node_child("Language");
	if (lang) {
		try {
			_language = dcp::LanguageTag(lang->content());
			auto additional = lang->optional_bool_attribute("Additional");
			if (!additional) {
				additional = lang->optional_bool_attribute("additional");
			}
			_language_is_additional = additional.get_value_or(false);
		} catch (dcp::LanguageTagError&) {
			/* The language tag can be empty or invalid if it was loaded from a
			 * 2.14.x metadata file; we'll just ignore it in that case.
			 */
			if (version <= 37) {
				if (!lang->content().empty()) {
					notes.push_back(fmt::format(
						_("A subtitle or closed caption file in this project is marked with the language '{}', "
						  "which {} does not recognise.  The file's language has been cleared."), lang->content(), variant::dcpomatic()));
				}
			} else {
				throw;
			}
		}
	}
}

TextContent::TextContent(Content* parent, vector<shared_ptr<Content>> c)
	: ContentPart(parent)
{
	/* This constructor is for join which is only supported for content types
	   that have a single text, so we can use only_text() here.
	*/
	auto ref = c[0]->only_text();
	DCPOMATIC_ASSERT(ref);
	auto ref_fonts = ref->fonts();

	for (size_t i = 1; i < c.size(); ++i) {

		if (c[i]->only_text()->use() != ref->use()) {
			throw JoinError(_("Content to be joined must have the same 'use subtitles' setting."));
		}

		if (c[i]->only_text()->burn() != ref->burn()) {
			throw JoinError(_("Content to be joined must have the same 'burn subtitles' setting."));
		}

		if (c[i]->only_text()->x_offset() != ref->x_offset()) {
			throw JoinError(_("Content to be joined must have the same subtitle X offset."));
		}

		if (c[i]->only_text()->y_offset() != ref->y_offset()) {
			throw JoinError(_("Content to be joined must have the same subtitle Y offset."));
		}

		if (c[i]->only_text()->x_scale() != ref->x_scale()) {
			throw JoinError(_("Content to be joined must have the same subtitle X scale."));
		}

		if (c[i]->only_text()->y_scale() != ref->y_scale()) {
			throw JoinError(_("Content to be joined must have the same subtitle Y scale."));
		}

		if (c[i]->only_text()->line_spacing() != ref->line_spacing()) {
			throw JoinError(_("Content to be joined must have the same subtitle line spacing."));
		}

		if ((c[i]->only_text()->fade_in() != ref->fade_in()) || (c[i]->only_text()->fade_out() != ref->fade_out())) {
			throw JoinError(_("Content to be joined must have the same subtitle fades."));
		}

		if ((c[i]->only_text()->outline_width() != ref->outline_width())) {
			throw JoinError(_("Content to be joined must have the same outline width."));
		}

		auto fonts = c[i]->only_text()->fonts();
		if (fonts.size() != ref_fonts.size()) {
			throw JoinError(_("Content to be joined must use the same fonts."));
		}

		if (c[i]->only_text()->dcp_track() != ref->dcp_track()) {
			throw JoinError(_("Content to be joined must use the same DCP track."));
		}

		if (c[i]->only_text()->language() != ref->language()) {
			throw JoinError(_("Content to be joined must use the same text language."));
		}

		if (c[i]->only_text()->language_is_additional() != ref->language_is_additional()) {
			throw JoinError(_("Content to be joined must both be main subtitle languages or both additional."));
		}

		auto j = ref_fonts.begin();
		auto k = fonts.begin();

		while (j != ref_fonts.end()) {
			if (**j != **k) {
				throw JoinError(_("Content to be joined must use the same fonts."));
			}
			++j;
			++k;
		}
	}

	_use = ref->use();
	_burn = ref->burn();
	_x_offset = ref->x_offset();
	_y_offset = ref->y_offset();
	_x_scale = ref->x_scale();
	_y_scale = ref->y_scale();
	_fonts = ref_fonts;
	_line_spacing = ref->line_spacing();
	_fade_in = ref->fade_in();
	_fade_out = ref->fade_out();
	_outline_width = ref->outline_width();
	_type = ref->type();
	_original_type = ref->original_type();
	_dcp_track = ref->dcp_track();
	_language = ref->language();
	_language_is_additional = ref->language_is_additional();

	connect_to_fonts();
}

/** _mutex must not be held on entry */
void
TextContent::as_xml(xmlpp::Element* root) const
{
	boost::mutex::scoped_lock lm(_mutex);

	auto text = cxml::add_child(root, "Text");

	cxml::add_text_child(text, "Use", _use ? "1" : "0");
	cxml::add_text_child(text, "Burn", _burn ? "1" : "0");
	cxml::add_text_child(text, "XOffset", fmt::to_string(_x_offset));
	cxml::add_text_child(text, "YOffset", fmt::to_string(_y_offset));
	cxml::add_text_child(text, "XScale", fmt::to_string(_x_scale));
	cxml::add_text_child(text, "YScale", fmt::to_string(_y_scale));
	if (_colour) {
		cxml::add_text_child(text, "Red", fmt::to_string(_colour->r));
		cxml::add_text_child(text, "Green", fmt::to_string(_colour->g));
		cxml::add_text_child(text, "Blue", fmt::to_string(_colour->b));
	}
	if (_effect) {
		switch (*_effect) {
		case dcp::Effect::NONE:
			cxml::add_text_child(text, "Effect", "none");
			break;
		case dcp::Effect::BORDER:
			cxml::add_text_child(text, "Effect", "outline");
			break;
		case dcp::Effect::SHADOW:
			cxml::add_text_child(text, "Effect", "shadow");
			break;
		}
	}
	if (_effect_colour) {
		cxml::add_text_child(text, "EffectRed", fmt::to_string(_effect_colour->r));
		cxml::add_text_child(text, "EffectGreen", fmt::to_string(_effect_colour->g));
		cxml::add_text_child(text, "EffectBlue", fmt::to_string(_effect_colour->b));
	}
	cxml::add_text_child(text, "LineSpacing", fmt::to_string(_line_spacing));
	if (_fade_in) {
		cxml::add_text_child(text, "FadeIn", fmt::to_string(_fade_in->get()));
	}
	if (_fade_out) {
		cxml::add_text_child(text, "FadeOut", fmt::to_string(_fade_out->get()));
	}
	cxml::add_text_child(text, "OutlineWidth", fmt::to_string(_outline_width));

	for (auto i: _fonts) {
		i->as_xml(cxml::add_child(text, "Font"));
	}

	cxml::add_text_child(text, "Type", text_type_to_string(_type));
	cxml::add_text_child(text, "OriginalType", text_type_to_string(_original_type));
	if (_dcp_track) {
		_dcp_track->as_xml(cxml::add_child(text, "DCPTrack"));
	}
	if (_language) {
		auto lang = cxml::add_child(text, "Language");
		lang->add_child_text(_language->as_string());
		lang->set_attribute("additional", _language_is_additional ? "1" : "0");
	}
}

string
TextContent::identifier() const
{
	auto s = fmt::to_string(x_scale())
		+ "_" + fmt::to_string(y_scale())
		+ "_" + fmt::to_string(x_offset())
		+ "_" + fmt::to_string(y_offset())
		+ "_" + fmt::to_string(line_spacing())
		+ "_" + fmt::to_string(fade_in().get_value_or(ContentTime()).get())
		+ "_" + fmt::to_string(fade_out().get_value_or(ContentTime()).get())
		+ "_" + fmt::to_string(outline_width())
		+ "_" + fmt::to_string(colour().get_value_or(dcp::Colour(255, 255, 255)).to_argb_string())
		+ "_" + fmt::to_string(dcp::effect_to_string(effect().get_value_or(dcp::Effect::NONE)))
		+ "_" + fmt::to_string(effect_colour().get_value_or(dcp::Colour(0, 0, 0)).to_argb_string())
		+ "_" + fmt::to_string(_parent->video_frame_rate().get_value_or(0));

	/* XXX: I suppose really _fonts shouldn't be in here, since not all
	   types of subtitle content involve fonts.
	*/
	for (auto f: _fonts) {
		s += "_" + f->file().get_value_or("Default").string();
	}

	/* The DCP track and language are for metadata only, and don't affect how this content looks */

	return s;
}

void
TextContent::add_font(shared_ptr<Font> font)
{
	boost::mutex::scoped_lock lm(_mutex);

	DCPOMATIC_ASSERT(!get_font_unlocked(font->id()));
	_fonts.push_back(font);
	connect_to_fonts();
}

void
TextContent::connect_to_fonts()
{
	for (auto const& i: _font_connections) {
		i.disconnect();
	}

	_font_connections.clear();

	for (auto i: _fonts) {
		_font_connections.push_back(i->Changed.connect(boost::bind(&TextContent::font_changed, this)));
	}
}

void
TextContent::font_changed()
{
	/* XXX: too late */
	ContentChangeSignaller cc(_parent, TextContentProperty::FONTS);
}

void
TextContent::set_colour(dcp::Colour colour)
{
	maybe_set(_colour, colour, TextContentProperty::COLOUR);
}

void
TextContent::unset_colour()
{
	maybe_set(_colour, optional<dcp::Colour>(), TextContentProperty::COLOUR);
}

void
TextContent::set_effect(dcp::Effect e)
{
	maybe_set(_effect, e, TextContentProperty::EFFECT);
}

void
TextContent::unset_effect()
{
	maybe_set(_effect, optional<dcp::Effect>(), TextContentProperty::EFFECT);
}

void
TextContent::set_effect_colour(dcp::Colour colour)
{
	maybe_set(_effect_colour, colour, TextContentProperty::EFFECT_COLOUR);
}

void
TextContent::unset_effect_colour()
{
	maybe_set(_effect_colour, optional<dcp::Colour>(), TextContentProperty::EFFECT_COLOUR);
}

void
TextContent::set_use(bool u)
{
	maybe_set(_use, u, TextContentProperty::USE);
}

void
TextContent::set_burn(bool b)
{
	maybe_set(_burn, b, TextContentProperty::BURN);
}

void
TextContent::set_x_offset(double o)
{
	maybe_set(_x_offset, o, TextContentProperty::X_OFFSET);
}

void
TextContent::set_y_offset(double o)
{
	maybe_set(_y_offset, o, TextContentProperty::Y_OFFSET);
}

void
TextContent::set_x_scale(double s)
{
	maybe_set(_x_scale, s, TextContentProperty::X_SCALE);
}

void
TextContent::set_y_scale(double s)
{
	maybe_set(_y_scale, s, TextContentProperty::Y_SCALE);
}

void
TextContent::set_line_spacing(double s)
{
	maybe_set(_line_spacing, s, TextContentProperty::LINE_SPACING);
}

void
TextContent::set_fade_in(ContentTime t)
{
	maybe_set(_fade_in, t, TextContentProperty::FADE_IN);
}

void
TextContent::unset_fade_in()
{
	maybe_set(_fade_in, optional<ContentTime>(), TextContentProperty::FADE_IN);
}

void
TextContent::set_fade_out(ContentTime t)
{
	maybe_set(_fade_out, t, TextContentProperty::FADE_OUT);
}

void
TextContent::unset_fade_out()
{
	maybe_set(_fade_out, optional<ContentTime>(), TextContentProperty::FADE_OUT);
}

void
TextContent::set_type(TextType type)
{
	maybe_set(_type, type, TextContentProperty::TYPE);
}

void
TextContent::set_outline_width(int w)
{
	maybe_set(_outline_width, w, TextContentProperty::OUTLINE_WIDTH);
}

void
TextContent::set_dcp_track(DCPTextTrack t)
{
	maybe_set(_dcp_track, t, TextContentProperty::DCP_TRACK);
}

void
TextContent::unset_dcp_track()
{
	maybe_set(_dcp_track, optional<DCPTextTrack>(), TextContentProperty::DCP_TRACK);
}

void
TextContent::set_language(optional<dcp::LanguageTag> language)
{
	maybe_set(_language, language, TextContentProperty::LANGUAGE);
}

void
TextContent::set_language_is_additional(bool additional)
{
	maybe_set(_language_is_additional, additional, TextContentProperty::LANGUAGE_IS_ADDITIONAL);
}

void
TextContent::take_settings_from(shared_ptr<const TextContent> c)
{
	set_use(c->_use);
	set_burn(c->_burn);
	set_x_offset(c->_x_offset);
	set_y_offset(c->_y_offset);
	set_x_scale(c->_x_scale);
	set_y_scale(c->_y_scale);
	maybe_set(_fonts, c->_fonts, TextContentProperty::FONTS);
	if (c->_colour) {
		set_colour(*c->_colour);
	} else {
		unset_colour();
	}
	if (c->_effect) {
		set_effect(*c->_effect);
	}
	if (c->_effect_colour) {
		set_effect_colour(*c->_effect_colour);
	} else {
		unset_effect_colour();
	}
	set_line_spacing(c->_line_spacing);
	if (c->_fade_in) {
		set_fade_in(*c->_fade_in);
	}
	if (c->_fade_out) {
		set_fade_out(*c->_fade_out);
	}
	set_outline_width(c->_outline_width);
	if (c->_dcp_track) {
		set_dcp_track(c->_dcp_track.get());
	} else {
		unset_dcp_track();
	}
	set_language(c->_language);
	set_language_is_additional(c->_language_is_additional);
}


shared_ptr<dcpomatic::Font>
TextContent::get_font(string id) const
{
	boost::mutex::scoped_lock lm(_mutex);
	return get_font_unlocked(id);
}


shared_ptr<dcpomatic::Font>
TextContent::get_font_unlocked(string id) const
{
	auto iter = std::find_if(_fonts.begin(), _fonts.end(), [&id](shared_ptr<dcpomatic::Font> font) {
		return font->id() == id;
	});

	if (iter == _fonts.end()) {
		return {};
	}

	return *iter;
}


void
TextContent::clear_fonts()
{
	boost::mutex::scoped_lock lm(_mutex);

	_fonts.clear();
}

