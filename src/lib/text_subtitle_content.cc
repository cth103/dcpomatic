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

TextSubtitleContent::TextSubtitleContent (shared_ptr<const Film> film, boost::filesystem::path path)
	: Content (film, path)
{
	subtitle.reset (new SubtitleContent (this, film));
}

TextSubtitleContent::TextSubtitleContent (shared_ptr<const Film> film, cxml::ConstNodePtr node, int version)
	: Content (film, node)
	, _length (node->number_child<ContentTime::Type> ("Length"))
{
	subtitle.reset (new SubtitleContent (this, film, node, version));
}

void
TextSubtitleContent::examine (boost::shared_ptr<Job> job)
{
	Content::examine (job);
	TextSubtitle s (shared_from_this ());

	/* Default to turning these subtitles on */
	subtitle->set_use_subtitles (true);

	boost::mutex::scoped_lock lm (_mutex);
	_length = s.length ();
	subtitle->add_font (shared_ptr<Font> (new Font (font_id)));
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
