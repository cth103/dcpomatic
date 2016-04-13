/*
    Copyright (C) 2014-2016 Carl Hetherington <cth@carlh.net>

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

#include "text_subtitle_content.h"
#include "util.h"
#include "text_subtitle.h"
#include "film.h"
#include "font.h"
#include "raw_convert.h"
#include "subtitle_content.h"
#include <libxml++/libxml++.h>
#include <iostream>

#include "i18n.h"

using std::string;
using std::cout;
using boost::shared_ptr;

std::string const TextSubtitleContent::font_id = "font";

int const TextSubtitleContentProperty::TEXT_SUBTITLE_COLOUR = 300;
int const TextSubtitleContentProperty::TEXT_SUBTITLE_OUTLINE = 301;
int const TextSubtitleContentProperty::TEXT_SUBTITLE_OUTLINE_COLOUR = 302;

TextSubtitleContent::TextSubtitleContent (shared_ptr<const Film> film, boost::filesystem::path path)
	: Content (film, path)
	, _colour (255, 255, 255)
	, _outline (false)
	, _outline_colour (0, 0, 0)
{
	subtitle.reset (new SubtitleContent (this, film, path));
}

TextSubtitleContent::TextSubtitleContent (shared_ptr<const Film> film, cxml::ConstNodePtr node, int version)
	: Content (film, node)
	, _length (node->number_child<ContentTime::Type> ("Length"))
	, _frame_rate (node->optional_number_child<double>("SubtitleVideoFrameRate"))
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
	subtitle.reset (new SubtitleContent (this, film, node, version));
}

void
TextSubtitleContent::examine (boost::shared_ptr<Job> job)
{
	Content::examine (job);
	TextSubtitle s (shared_from_this ());

	/* Default to turning these subtitles on */
	set_use_subtitles (true);

	boost::mutex::scoped_lock lm (_mutex);
	_length = s.length ();
	add_font (shared_ptr<Font> (new Font (font_id)));
}

string
TextSubtitleContent::summary () const
{
	return path_summary() + " " + _("[subtitles]");
}

string
TextSubtitleContent::technical_summary () const
{
	return Content::technical_summary() + " - " + _("Text subtitles");
}

void
TextSubtitleContent::as_xml (xmlpp::Node* node) const
{
	node->add_child("Type")->add_child_text ("TextSubtitle");
	Content::as_xml (node);
	subtitle->as_xml (node);
	node->add_child("Length")->add_child_text (raw_convert<string> (_length.get ()));
	if (_frame_rate) {
		node->add_child("SubtitleVideoFrameRate")->add_child_text (raw_convert<string> (_frame_rate.get()));
	}
	node->add_child("Red")->add_child_text (raw_convert<string> (_colour.r));
	node->add_child("Green")->add_child_text (raw_convert<string> (_colour.g));
	node->add_child("Blue")->add_child_text (raw_convert<string> (_colour.b));
	node->add_child("Outline")->add_child_text (raw_convert<string> (_outline));
	node->add_child("OutlineRed")->add_child_text (raw_convert<string> (_outline_colour.r));
	node->add_child("OutlineGreen")->add_child_text (raw_convert<string> (_outline_colour.g));
	node->add_child("OutlineBlue")->add_child_text (raw_convert<string> (_outline_colour.b));
}

DCPTime
TextSubtitleContent::full_length () const
{
	FrameRateChange const frc (subtitle_video_frame_rate(), film()->video_frame_rate ());
	return DCPTime (_length, frc);
}

void
TextSubtitleContent::set_subtitle_video_frame_rate (double r)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_frame_rate = r;
	}

	signal_changed (SubtitleContentProperty::SUBTITLE_VIDEO_FRAME_RATE);
}

double
TextSubtitleContent::subtitle_video_frame_rate () const
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
	return film()->active_frame_rate_change(position()).source;
}

void
TextSubtitleContent::set_colour (dcp::Colour colour)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_colour == colour) {
			return;
		}

		_colour = colour;
	}

	signal_changed (TextSubtitleContentProperty::TEXT_SUBTITLE_COLOUR);
}

void
TextSubtitleContent::set_outline (bool o)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_outline == o) {
			return;
		}

		_outline = o;
	}

	signal_changed (TextSubtitleContentProperty::TEXT_SUBTITLE_OUTLINE);
}

void
TextSubtitleContent::set_outline_colour (dcp::Colour colour)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_outline_colour == colour) {
			return;
		}

		_outline_colour = colour;
	}

	signal_changed (TextSubtitleContentProperty::TEXT_SUBTITLE_OUTLINE_COLOUR);
}
