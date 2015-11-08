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

#include "subrip_content.h"
#include "util.h"
#include "subrip.h"
#include "film.h"
#include "font.h"
#include "raw_convert.h"
#include <libxml++/libxml++.h>
#include <iostream>

#include "i18n.h"

using std::string;
using std::cout;
using boost::shared_ptr;
using boost::lexical_cast;

std::string const SubRipContent::font_id = "font";

int const SubRipContentProperty::SUBTITLE_COLOUR = 300;
int const SubRipContentProperty::SUBTITLE_OUTLINE = 301;
int const SubRipContentProperty::SUBTITLE_OUTLINE_COLOUR = 302;

SubRipContent::SubRipContent (shared_ptr<const Film> film, boost::filesystem::path path)
	: Content (film, path)
	, SubtitleContent (film, path)
	, _colour (255, 255, 255)
	, _outline (false)
	, _outline_colour (0, 0, 0)
{

}

SubRipContent::SubRipContent (shared_ptr<const Film> film, cxml::ConstNodePtr node, int version)
	: Content (film, node)
	, SubtitleContent (film, node, version)
	, _length (node->number_child<ContentTime::Type> ("Length"))
	, _frame_rate (node->optional_number_child<double>("SubtitleFrameRate"))
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

}

void
SubRipContent::examine (boost::shared_ptr<Job> job)
{
	Content::examine (job);
	SubRip s (shared_from_this ());

	/* Default to turning these subtitles on */
	set_use_subtitles (true);

	boost::mutex::scoped_lock lm (_mutex);
	_length = s.length ();
	add_font (shared_ptr<Font> (new Font (font_id)));
}

string
SubRipContent::summary () const
{
	return path_summary() + " " + _("[subtitles]");
}

string
SubRipContent::technical_summary () const
{
	return Content::technical_summary() + " - " + _("SubRip subtitles");
}

void
SubRipContent::as_xml (xmlpp::Node* node) const
{
	node->add_child("Type")->add_child_text ("SubRip");
	Content::as_xml (node);
	SubtitleContent::as_xml (node);
	node->add_child("Length")->add_child_text (raw_convert<string> (_length.get ()));
	node->add_child("Red")->add_child_text (raw_convert<string> (_colour.r));
	node->add_child("Green")->add_child_text (raw_convert<string> (_colour.g));
	node->add_child("Blue")->add_child_text (raw_convert<string> (_colour.b));
	node->add_child("Outline")->add_child_text (raw_convert<string> (_outline));
	node->add_child("OutlineRed")->add_child_text (raw_convert<string> (_outline_colour.r));
	node->add_child("OutlineGreen")->add_child_text (raw_convert<string> (_outline_colour.g));
	node->add_child("OutlineBlue")->add_child_text (raw_convert<string> (_outline_colour.b));
}

DCPTime
SubRipContent::full_length () const
{
	FrameRateChange const frc (subtitle_video_frame_rate(), film()->video_frame_rate ());
	return DCPTime (_length, frc);
}

void
SubRipContent::set_subtitle_video_frame_rate (int r)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_frame_rate = r;
	}

	signal_changed (SubtitleContentProperty::SUBTITLE_VIDEO_FRAME_RATE);
}

double
SubRipContent::subtitle_video_frame_rate () const
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
SubRipContent::set_colour (dcp::Colour colour)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_colour == colour) {
			return;
		}

		_colour = colour;
	}

	signal_changed (SubRipContentProperty::SUBTITLE_COLOUR);
}

void
SubRipContent::set_outline (bool o)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_outline == o) {
			return;
		}

		_outline = o;
	}

	signal_changed (SubRipContentProperty::SUBTITLE_OUTLINE);
}

void
SubRipContent::set_outline_colour (dcp::Colour colour)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_outline_colour == colour) {
			return;
		}

		_outline_colour = colour;
	}

	signal_changed (SubRipContentProperty::SUBTITLE_OUTLINE_COLOUR);
}
