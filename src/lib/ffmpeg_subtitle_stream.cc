/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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
#include <iostream>

using std::string;
using std::map;
using std::list;
using std::cout;

/** Construct a SubtitleStream from a value returned from to_string().
 *  @param t String returned from to_string().
 *  @param v State file version.
 */
FFmpegSubtitleStream::FFmpegSubtitleStream (cxml::ConstNodePtr node, int version)
	: FFmpegStream (node)
{
	if (version == 32) {
		BOOST_FOREACH (cxml::NodePtr i, node->node_children ("Period")) {
			/* In version 32 we assumed that from times were unique, so they weer
			   used as identifiers.
			*/
			add_subtitle (
				raw_convert<string> (i->string_child ("From")),
				ContentTimePeriod (
					ContentTime (i->number_child<ContentTime::Type> ("From")),
					ContentTime (i->number_child<ContentTime::Type> ("To"))
					)
				);
		}
	} else {
		/* In version 33 we use a hash of various parts of the subtitle as the id */
		BOOST_FOREACH (cxml::NodePtr i, node->node_children ("Subtitle")) {
			add_subtitle (
				raw_convert<string> (i->string_child ("Id")),
				ContentTimePeriod (
					ContentTime (i->number_child<ContentTime::Type> ("From")),
					ContentTime (i->number_child<ContentTime::Type> ("To"))
					)
				);
		}
	}
}

void
FFmpegSubtitleStream::as_xml (xmlpp::Node* root) const
{
	FFmpegStream::as_xml (root);

	for (map<string, ContentTimePeriod>::const_iterator i = _subtitles.begin(); i != _subtitles.end(); ++i) {
		xmlpp::Node* node = root->add_child ("Subtitle");
		node->add_child("Id")->add_child_text (i->first);
		node->add_child("From")->add_child_text (raw_convert<string> (i->second.from.get ()));
		node->add_child("To")->add_child_text (raw_convert<string> (i->second.to.get ()));
	}
}

void
FFmpegSubtitleStream::add_subtitle (string id, ContentTimePeriod period)
{
	DCPOMATIC_ASSERT (_subtitles.find (id) == _subtitles.end ());
	_subtitles[id] = period;
}

list<ContentTimePeriod>
FFmpegSubtitleStream::subtitles_during (ContentTimePeriod period, bool starting) const
{
	list<ContentTimePeriod> d;

	/* XXX: inefficient */
	for (map<string, ContentTimePeriod>::const_iterator i = _subtitles.begin(); i != _subtitles.end(); ++i) {
		if ((starting && period.contains (i->second.from)) || (!starting && period.overlaps (i->second))) {
			d.push_back (i->second);
		}
	}

	return d;
}

ContentTime
FFmpegSubtitleStream::find_subtitle_to (string id) const
{
	map<string, ContentTimePeriod>::const_iterator i = _subtitles.find (id);
	DCPOMATIC_ASSERT (i != _subtitles.end ());
	return i->second.to;
}

/** Add some offset to all the times in the stream */
void
FFmpegSubtitleStream::add_offset (ContentTime offset)
{
	for (map<string, ContentTimePeriod>::iterator i = _subtitles.begin(); i != _subtitles.end(); ++i) {
		i->second.from += offset;
		i->second.to += offset;
	}
}
