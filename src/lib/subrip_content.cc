/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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
#include <dcp/raw_convert.h>

#include "i18n.h"

using std::string;
using std::cout;
using dcp::raw_convert;
using boost::shared_ptr;
using boost::lexical_cast;

std::string const SubRipContent::font_id = "font";

SubRipContent::SubRipContent (shared_ptr<const Film> film, boost::filesystem::path path)
	: Content (film, path)
	, SubtitleContent (film, path)
{

}

SubRipContent::SubRipContent (shared_ptr<const Film> film, cxml::ConstNodePtr node, int version)
	: Content (film, node)
	, SubtitleContent (film, node, version)
	, _length (node->number_child<DCPTime::Type> ("Length"))
{

}

void
SubRipContent::examine (boost::shared_ptr<Job> job, bool calculate_digest)
{
	Content::examine (job, calculate_digest);
	SubRip s (shared_from_this ());

	shared_ptr<const Film> film = _film.lock ();
	DCPOMATIC_ASSERT (film);
	
	DCPTime len (s.length (), film->active_frame_rate_change (position ()));

	boost::mutex::scoped_lock lm (_mutex);
	_length = len;
	_fonts.push_back (shared_ptr<Font> (new Font (font_id)));
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
}

DCPTime
SubRipContent::full_length () const
{
	/* XXX: this assumes that the timing of the SubRip file is appropriate
	   for the DCP's frame rate.
	*/
	return _length;
}
