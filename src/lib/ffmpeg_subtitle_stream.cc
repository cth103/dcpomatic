/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#include "ffmpeg_subtitle_stream.h"
#include <dcp/raw_convert.h>
#include <libxml++/libxml++.h>
#include <boost/foreach.hpp>
#include <iostream>

using std::string;
using std::map;
using std::list;
using std::cout;
using std::make_pair;
using dcp::raw_convert;

/** Construct a SubtitleStream from a value returned from to_string().
 *  @param t String returned from to_string().
 *  @param v State file version.
 */
FFmpegSubtitleStream::FFmpegSubtitleStream (cxml::ConstNodePtr node, int version)
	: FFmpegStream (node)
{
	if (version == 32) {
		BOOST_FOREACH (cxml::NodePtr i, node->node_children ("Period")) {
			/* In version 32 we assumed that from times were unique, so they were
			   used as identifiers.  All subtitles were image subtitles.
			*/
			add_image_subtitle (
				raw_convert<string> (i->string_child ("From")),
				ContentTimePeriod (
					ContentTime (i->number_child<ContentTime::Type> ("From")),
					ContentTime (i->number_child<ContentTime::Type> ("To"))
					)
				);
		}
	} else {
		/* In version 33 we use a hash of various parts of the subtitle as the id.
		   <Subtitle> was initially used for image subtitles; later we have
		   <ImageSubtitle> and <TextSubtitle>
		*/
		BOOST_FOREACH (cxml::NodePtr i, node->node_children ("Subtitle")) {
			add_image_subtitle (
				raw_convert<string> (i->string_child ("Id")),
				ContentTimePeriod (
					ContentTime (i->number_child<ContentTime::Type> ("From")),
					ContentTime (i->number_child<ContentTime::Type> ("To"))
					)
				);
		}

		BOOST_FOREACH (cxml::NodePtr i, node->node_children ("ImageSubtitle")) {
			add_image_subtitle (
				raw_convert<string> (i->string_child ("Id")),
				ContentTimePeriod (
					ContentTime (i->number_child<ContentTime::Type> ("From")),
					ContentTime (i->number_child<ContentTime::Type> ("To"))
					)
				);
		}

		BOOST_FOREACH (cxml::NodePtr i, node->node_children ("TextSubtitle")) {
			add_text_subtitle (
				raw_convert<string> (i->string_child ("Id")),
				ContentTimePeriod (
					ContentTime (i->number_child<ContentTime::Type> ("From")),
					ContentTime (i->number_child<ContentTime::Type> ("To"))
					)
				);
		}

		BOOST_FOREACH (cxml::NodePtr i, node->node_children ("Colour")) {
			_colours[RGBA(i->node_child("From"))] = RGBA (i->node_child("To"));
		}
	}
}

void
FFmpegSubtitleStream::as_xml (xmlpp::Node* root) const
{
	FFmpegStream::as_xml (root);

	as_xml (root, _image_subtitles, "ImageSubtitle");
	as_xml (root, _text_subtitles, "TextSubtitle");

	for (map<RGBA, RGBA>::const_iterator i = _colours.begin(); i != _colours.end(); ++i) {
		xmlpp::Node* node = root->add_child("Colour");
		i->first.as_xml (node->add_child("From"));
		i->second.as_xml (node->add_child("To"));
	}
}

void
FFmpegSubtitleStream::as_xml (xmlpp::Node* root, PeriodMap const & subs, string node_name) const
{
	for (PeriodMap::const_iterator i = subs.begin(); i != subs.end(); ++i) {
		xmlpp::Node* node = root->add_child (node_name);
		node->add_child("Id")->add_child_text (i->first);
		node->add_child("From")->add_child_text (raw_convert<string> (i->second.from.get ()));
		node->add_child("To")->add_child_text (raw_convert<string> (i->second.to.get ()));
	}
}

void
FFmpegSubtitleStream::add_image_subtitle (string id, ContentTimePeriod period)
{
	DCPOMATIC_ASSERT (_image_subtitles.find (id) == _image_subtitles.end ());
	_image_subtitles[id] = period;
}

void
FFmpegSubtitleStream::add_text_subtitle (string id, ContentTimePeriod period)
{
	DCPOMATIC_ASSERT (_text_subtitles.find (id) == _text_subtitles.end ());
	_text_subtitles[id] = period;
}

list<ContentTimePeriod>
FFmpegSubtitleStream::image_subtitles_during (ContentTimePeriod period, bool starting) const
{
	return subtitles_during (period, starting, _image_subtitles);
}

list<ContentTimePeriod>
FFmpegSubtitleStream::text_subtitles_during (ContentTimePeriod period, bool starting) const
{
	return subtitles_during (period, starting, _text_subtitles);
}

struct PeriodSorter
{
	bool operator() (ContentTimePeriod const & a, ContentTimePeriod const & b) {
		return a.from < b.from;
	}
};

list<ContentTimePeriod>
FFmpegSubtitleStream::subtitles_during (ContentTimePeriod period, bool starting, PeriodMap const & subs) const
{
	list<ContentTimePeriod> d;

	/* XXX: inefficient */
	for (map<string, ContentTimePeriod>::const_iterator i = subs.begin(); i != subs.end(); ++i) {
		if ((starting && period.contains(i->second.from)) || (!starting && period.overlap(i->second))) {
			d.push_back (i->second);
		}
	}

	d.sort (PeriodSorter ());

	return d;
}

ContentTime
FFmpegSubtitleStream::find_subtitle_to (string id) const
{
	PeriodMap::const_iterator i = _image_subtitles.find (id);
	if (i != _image_subtitles.end ()) {
		return i->second.to;
	}

	i = _text_subtitles.find (id);
	DCPOMATIC_ASSERT (i != _text_subtitles.end ());
	return i->second.to;
}

/** Add some offset to all the times in the stream */
void
FFmpegSubtitleStream::add_offset (ContentTime offset)
{
	for (PeriodMap::iterator i = _image_subtitles.begin(); i != _image_subtitles.end(); ++i) {
		i->second.from += offset;
		i->second.to += offset;
	}

	for (PeriodMap::iterator i = _text_subtitles.begin(); i != _text_subtitles.end(); ++i) {
		i->second.from += offset;
		i->second.to += offset;
	}
}

map<RGBA, RGBA>
FFmpegSubtitleStream::colours () const
{
	return _colours;
}

void
FFmpegSubtitleStream::set_colour (RGBA from, RGBA to)
{
	_colours[from] = to;
}

bool
FFmpegSubtitleStream::has_text () const
{
	return !_text_subtitles.empty ();
}

bool
FFmpegSubtitleStream::has_image () const
{
	return !_image_subtitles.empty ();
}
