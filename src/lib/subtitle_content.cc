/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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

#include <libcxml/cxml.h>
#include <dcp/raw_convert.h>
#include "subtitle_content.h"
#include "util.h"
#include "exceptions.h"
#include "safe_stringstream.h"
#include "font.h"

#include "i18n.h"

using std::string;
using std::vector;
using std::cout;
using std::list;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using dcp::raw_convert;

int const SubtitleContentProperty::SUBTITLE_X_OFFSET = 500;
int const SubtitleContentProperty::SUBTITLE_Y_OFFSET = 501;
int const SubtitleContentProperty::SUBTITLE_X_SCALE = 502;
int const SubtitleContentProperty::SUBTITLE_Y_SCALE = 503;
int const SubtitleContentProperty::USE_SUBTITLES = 504;
int const SubtitleContentProperty::SUBTITLE_LANGUAGE = 505;
int const SubtitleContentProperty::FONTS = 506;

SubtitleContent::SubtitleContent (shared_ptr<const Film> f)
	: Content (f)
	, _use_subtitles (false)
	, _subtitle_x_offset (0)
	, _subtitle_y_offset (0)
	, _subtitle_x_scale (1)
	, _subtitle_y_scale (1)
{

}

SubtitleContent::SubtitleContent (shared_ptr<const Film> f, boost::filesystem::path p)
	: Content (f, p)
	, _use_subtitles (false)
	, _subtitle_x_offset (0)
	, _subtitle_y_offset (0)
	, _subtitle_x_scale (1)
	, _subtitle_y_scale (1)
{

}

SubtitleContent::SubtitleContent (shared_ptr<const Film> f, cxml::ConstNodePtr node, int version)
	: Content (f, node)
	, _use_subtitles (false)
	, _subtitle_x_offset (0)
	, _subtitle_y_offset (0)
	, _subtitle_x_scale (1)
	, _subtitle_y_scale (1)
{
	if (version >= 32) {
		_use_subtitles = node->bool_child ("UseSubtitles");
	} else {
		_use_subtitles = false;
	}
	
	if (version >= 7) {
		_subtitle_x_offset = node->number_child<float> ("SubtitleXOffset");
		_subtitle_y_offset = node->number_child<float> ("SubtitleYOffset");
	} else {
		_subtitle_y_offset = node->number_child<float> ("SubtitleOffset");
	}

	if (version >= 10) {
		_subtitle_x_scale = node->number_child<float> ("SubtitleXScale");
		_subtitle_y_scale = node->number_child<float> ("SubtitleYScale");
	} else {
		_subtitle_x_scale = _subtitle_y_scale = node->number_child<float> ("SubtitleScale");
	}

	_subtitle_language = node->optional_string_child ("SubtitleLanguage").get_value_or ("");

	list<cxml::NodePtr> fonts = node->node_children ("Font");
	for (list<cxml::NodePtr>::const_iterator i = fonts.begin(); i != fonts.end(); ++i) {
		_fonts.push_back (shared_ptr<Font> (new Font (*i)));
	}
}

SubtitleContent::SubtitleContent (shared_ptr<const Film> f, vector<shared_ptr<Content> > c)
	: Content (f, c)
{
	shared_ptr<SubtitleContent> ref = dynamic_pointer_cast<SubtitleContent> (c[0]);
	DCPOMATIC_ASSERT (ref);
	list<shared_ptr<Font> > ref_fonts = ref->fonts ();
	
	for (size_t i = 0; i < c.size(); ++i) {
		shared_ptr<SubtitleContent> sc = dynamic_pointer_cast<SubtitleContent> (c[i]);

		if (sc->use_subtitles() != ref->use_subtitles()) {
			throw JoinError (_("Content to be joined must have the same 'use subtitles' setting."));
		}

		if (sc->subtitle_x_offset() != ref->subtitle_x_offset()) {
			throw JoinError (_("Content to be joined must have the same subtitle X offset."));
		}
		
		if (sc->subtitle_y_offset() != ref->subtitle_y_offset()) {
			throw JoinError (_("Content to be joined must have the same subtitle Y offset."));
		}

		if (sc->subtitle_x_scale() != ref->subtitle_x_scale()) {
			throw JoinError (_("Content to be joined must have the same subtitle X scale."));
		}

		if (sc->subtitle_y_scale() != ref->subtitle_y_scale()) {
			throw JoinError (_("Content to be joined must have the same subtitle Y scale."));
		}

		list<shared_ptr<Font> > fonts = sc->fonts ();
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
	_subtitle_x_offset = ref->subtitle_x_offset ();
	_subtitle_y_offset = ref->subtitle_y_offset ();
	_subtitle_x_scale = ref->subtitle_x_scale ();
	_subtitle_y_scale = ref->subtitle_y_scale ();
	_subtitle_language = ref->subtitle_language ();
	_fonts = ref_fonts;
}

/** _mutex must not be held on entry */
void
SubtitleContent::as_xml (xmlpp::Node* root) const
{
	boost::mutex::scoped_lock lm (_mutex);
	
	root->add_child("UseSubtitles")->add_child_text (raw_convert<string> (_use_subtitles));
	root->add_child("SubtitleXOffset")->add_child_text (raw_convert<string> (_subtitle_x_offset));
	root->add_child("SubtitleYOffset")->add_child_text (raw_convert<string> (_subtitle_y_offset));
	root->add_child("SubtitleXScale")->add_child_text (raw_convert<string> (_subtitle_x_scale));
	root->add_child("SubtitleYScale")->add_child_text (raw_convert<string> (_subtitle_y_scale));
	root->add_child("SubtitleLanguage")->add_child_text (_subtitle_language);

	for (list<shared_ptr<Font> >::const_iterator i = _fonts.begin(); i != _fonts.end(); ++i) {
		(*i)->as_xml (root->add_child("Font"));
	}
}

void
SubtitleContent::set_use_subtitles (bool u)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_use_subtitles = u;
	}
	signal_changed (SubtitleContentProperty::USE_SUBTITLES);
}
	
void
SubtitleContent::set_subtitle_x_offset (double o)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_subtitle_x_offset = o;
	}
	signal_changed (SubtitleContentProperty::SUBTITLE_X_OFFSET);
}

void
SubtitleContent::set_subtitle_y_offset (double o)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_subtitle_y_offset = o;
	}
	signal_changed (SubtitleContentProperty::SUBTITLE_Y_OFFSET);
}

void
SubtitleContent::set_subtitle_x_scale (double s)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_subtitle_x_scale = s;
	}
	signal_changed (SubtitleContentProperty::SUBTITLE_X_SCALE);
}

void
SubtitleContent::set_subtitle_y_scale (double s)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_subtitle_y_scale = s;
	}
	signal_changed (SubtitleContentProperty::SUBTITLE_Y_SCALE);
}

void
SubtitleContent::set_subtitle_language (string language)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_subtitle_language = language;
	}
	signal_changed (SubtitleContentProperty::SUBTITLE_LANGUAGE);
}

string
SubtitleContent::identifier () const
{
	SafeStringStream s;
	s << Content::identifier()
	  << "_" << raw_convert<string> (subtitle_x_scale())
	  << "_" << raw_convert<string> (subtitle_y_scale())
	  << "_" << raw_convert<string> (subtitle_x_offset())
	  << "_" << raw_convert<string> (subtitle_y_offset());

	/* The language is for metadata only, and doesn't affect
	   how this content looks.
	*/

	return s.str ();
}
