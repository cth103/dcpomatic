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

#include "text_text_content.h"
#include "util.h"
#include "text_subtitle.h"
#include "film.h"
#include "font.h"
#include "text_content.h"
#include <dcp/raw_convert.h>
#include <libxml++/libxml++.h>
#include <iostream>

#include "i18n.h"

using std::string;
using std::cout;
using boost::shared_ptr;
using dcp::raw_convert;

TextTextContent::TextTextContent (shared_ptr<const Film> film, boost::filesystem::path path)
	: Content (film, path)
{
	subtitle.reset (new TextContent (this));
}

TextTextContent::TextTextContent (shared_ptr<const Film> film, cxml::ConstNodePtr node, int version)
	: Content (film, node)
	, _length (node->number_child<ContentTime::Type> ("Length"))
{
	subtitle = TextContent::from_xml (this, node, version);
}

void
TextTextContent::examine (boost::shared_ptr<Job> job)
{
	Content::examine (job);
	TextSubtitle s (shared_from_this ());

	/* Default to turning these subtitles on */
	subtitle->set_use (true);

	boost::mutex::scoped_lock lm (_mutex);
	_length = s.length ();
	subtitle->add_font (shared_ptr<Font> (new Font (TEXT_FONT_ID)));
}

string
TextTextContent::summary () const
{
	return path_summary() + " " + _("[subtitles]");
}

string
TextTextContent::technical_summary () const
{
	return Content::technical_summary() + " - " + _("Text subtitles");
}

void
TextTextContent::as_xml (xmlpp::Node* node, bool with_paths) const
{
	node->add_child("Type")->add_child_text ("TextSubtitle");
	Content::as_xml (node, with_paths);

	if (subtitle) {
		subtitle->as_xml (node);
	}

	node->add_child("Length")->add_child_text (raw_convert<string> (_length.get ()));
}

DCPTime
TextTextContent::full_length () const
{
	FrameRateChange const frc (active_video_frame_rate(), film()->video_frame_rate ());
	return DCPTime (_length, frc);
}
