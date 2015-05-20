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

#include "ffmpeg_subtitle_stream.h"
#include "raw_convert.h"
#include <libxml++/libxml++.h>
#include <boost/foreach.hpp>

using std::string;
using std::map;
using std::list;

/** Construct a SubtitleStream from a value returned from to_string().
 *  @param t String returned from to_string().
 *  @param v State file version.
 */
FFmpegSubtitleStream::FFmpegSubtitleStream (cxml::ConstNodePtr node)
	: FFmpegStream (node)
{
	BOOST_FOREACH (cxml::NodePtr i, node->node_children ("Period")) {
		add_subtitle (
			ContentTimePeriod (
				ContentTime (node->number_child<ContentTime::Type> ("From")),
				ContentTime (node->number_child<ContentTime::Type> ("To"))
				)
			);
	}
}

void
FFmpegSubtitleStream::as_xml (xmlpp::Node* root) const
{
	FFmpegStream::as_xml (root);

	for (map<ContentTime, ContentTime>::const_iterator i = _subtitles.begin(); i != _subtitles.end(); ++i) {
		xmlpp::Node* node = root->add_child ("Subtitle");
		node->add_child("From")->add_child_text (raw_convert<string> (i->first.get ()));
		node->add_child("To")->add_child_text (raw_convert<string> (i->second.get ()));
	}
}

void
FFmpegSubtitleStream::add_subtitle (ContentTimePeriod period)
{
	DCPOMATIC_ASSERT (_subtitles.find (period.from) == _subtitles.end ());
	_subtitles[period.from] = period.to;
}

list<ContentTimePeriod> 
FFmpegSubtitleStream::subtitles_during (ContentTimePeriod period, bool starting) const
{
	list<ContentTimePeriod> d;
	
	/* XXX: inefficient */
	for (map<ContentTime, ContentTime>::const_iterator i = _subtitles.begin(); i != _subtitles.end(); ++i) {
		if ((starting && period.contains (i->first)) || (!starting && period.overlaps (ContentTimePeriod (i->first, i->second)))) {
			d.push_back (ContentTimePeriod (i->first, i->second));
		}
	}

	return d;
}

ContentTime
FFmpegSubtitleStream::find_subtitle_to (ContentTime from) const
{
	map<ContentTime, ContentTime>::const_iterator i = _subtitles.find (from);
	DCPOMATIC_ASSERT (i != _subtitles.end ());
	return i->second;
}
