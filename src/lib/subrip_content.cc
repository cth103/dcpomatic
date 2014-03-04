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

#include "i18n.h"

using std::stringstream;
using std::string;
using std::cout;
using boost::shared_ptr;
using boost::lexical_cast;

SubRipContent::SubRipContent (shared_ptr<const Film> film, boost::filesystem::path path)
	: Content (film, path)
	, SubtitleContent (film, path)
{

}

SubRipContent::SubRipContent (shared_ptr<const Film> film, shared_ptr<const cxml::Node> node, int version)
	: Content (film, node)
	, SubtitleContent (film, node, version)
	, _length (node->number_child<int64_t> ("Length"))
{

}

void
SubRipContent::examine (boost::shared_ptr<Job> job)
{
	Content::examine (job);
	SubRip s (shared_from_this ());
	boost::mutex::scoped_lock lm (_mutex);
	shared_ptr<const Film> film = _film.lock ();
	_length = DCPTime (s.length (), film->active_frame_rate_change (position ()));
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

string
SubRipContent::information () const
{
	
}
	
void
SubRipContent::as_xml (xmlpp::Node* node) const
{
	LocaleGuard lg;
	
	node->add_child("Type")->add_child_text ("SubRip");
	Content::as_xml (node);
	SubtitleContent::as_xml (node);
	node->add_child("Length")->add_child_text (lexical_cast<string> (_length));
}

DCPTime
SubRipContent::full_length () const
{
	/* XXX: this assumes that the timing of the SubRip file is appropriate
	   for the DCP's frame rate.
	*/
	return _length;
}

string
SubRipContent::identifier () const
{
	LocaleGuard lg;

	stringstream s;
	s << Content::identifier()
	  << "_" << subtitle_scale()
	  << "_" << subtitle_x_offset()
	  << "_" << subtitle_y_offset();

	return s.str ();
}
