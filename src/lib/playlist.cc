/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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
#include <boost/shared_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include "playlist.h"
#include "sndfile_content.h"
#include "sndfile_decoder.h"
#include "video_content.h"
#include "ffmpeg_decoder.h"
#include "ffmpeg_content.h"
#include "still_image_decoder.h"
#include "still_image_content.h"
#include "content_factory.h"
#include "job.h"
#include "config.h"
#include "util.h"

#include "i18n.h"

using std::list;
using std::cout;
using std::vector;
using std::min;
using std::max;
using std::string;
using std::stringstream;
using std::pair;
using boost::optional;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;
using boost::lexical_cast;

Playlist::Playlist ()
	: _sequence_video (true)
	, _sequencing_video (false)
{

}

Playlist::~Playlist ()
{
	_content.clear ();
	reconnect ();
}

void
Playlist::content_changed (weak_ptr<Content> content, int property, bool frequent)
{
	if (property == ContentProperty::LENGTH) {
		maybe_sequence_video ();
	}
	
	ContentChanged (content, property, frequent);
}


void
Playlist::maybe_sequence_video ()
{
	if (!_sequence_video || _sequencing_video) {
		return;
	}
	
	_sequencing_video = true;
	
	ContentList cl = _content;
	sort (cl.begin(), cl.end(), ContentSorter ());
	Time last = 0;
	for (ContentList::iterator i = cl.begin(); i != cl.end(); ++i) {
		if (!dynamic_pointer_cast<VideoContent> (*i)) {
			continue;
		}
		
		(*i)->set_start (last);
		last = (*i)->end ();
	}
	
	_sequencing_video = false;
}

string
Playlist::video_identifier () const
{
	string t;
	
	for (ContentList::const_iterator i = _content.begin(); i != _content.end(); ++i) {
		shared_ptr<const VideoContent> vc = dynamic_pointer_cast<const VideoContent> (*i);
		if (vc) {
			t += vc->identifier ();
		}
	}

	return md5_digest (t.c_str(), t.length());
}

/** @param node <Playlist> node */
void
Playlist::set_from_xml (shared_ptr<const Film> film, shared_ptr<const cxml::Node> node)
{
	list<shared_ptr<cxml::Node> > c = node->node_children ("Content");
	for (list<shared_ptr<cxml::Node> >::iterator i = c.begin(); i != c.end(); ++i) {
		_content.push_back (content_factory (film, *i));
	}

	reconnect ();
	_sequence_video = node->bool_child ("SequenceVideo");
}

/** @param node <Playlist> node */
void
Playlist::as_xml (xmlpp::Node* node)
{
	for (ContentList::iterator i = _content.begin(); i != _content.end(); ++i) {
		(*i)->as_xml (node->add_child ("Content"));
	}

	node->add_child("SequenceVideo")->add_child_text(_sequence_video ? "1" : "0");
}

void
Playlist::add (shared_ptr<Content> c)
{
	_content.push_back (c);
	reconnect ();
	Changed ();
}

void
Playlist::remove (shared_ptr<Content> c)
{
	ContentList::iterator i = _content.begin ();
	while (i != _content.end() && *i != c) {
		++i;
	}
	
	if (i != _content.end ()) {
		_content.erase (i);
		Changed ();
	}
}

void
Playlist::remove (ContentList c)
{
	for (ContentList::iterator i = c.begin(); i != c.end(); ++i) {
		ContentList::iterator j = _content.begin ();
		while (j != _content.end() && *j != *i) {
			++j;
		}
	
		if (j != _content.end ()) {
			_content.erase (j);
		}
	}

	Changed ();
}

bool
Playlist::has_subtitles () const
{
	for (ContentList::const_iterator i = _content.begin(); i != _content.end(); ++i) {
		shared_ptr<const FFmpegContent> fc = dynamic_pointer_cast<FFmpegContent> (*i);
		if (fc && !fc->subtitle_streams().empty()) {
			return true;
		}
	}

	return false;
}

class FrameRateCandidate
{
public:
	FrameRateCandidate (float source_, int dcp_)
		: source (source_)
		, dcp (dcp_)
	{}

	float source;
	int dcp;
};

int
Playlist::best_dcp_frame_rate () const
{
	list<int> const allowed_dcp_frame_rates = Config::instance()->allowed_dcp_frame_rates ();

	/* Work out what rates we could manage, including those achieved by using skip / repeat. */
	list<FrameRateCandidate> candidates;

	/* Start with the ones without skip / repeat so they will get matched in preference to skipped/repeated ones */
	for (list<int>::const_iterator i = allowed_dcp_frame_rates.begin(); i != allowed_dcp_frame_rates.end(); ++i) {
		candidates.push_back (FrameRateCandidate (*i, *i));
	}

	/* Then the skip/repeat ones */
	for (list<int>::const_iterator i = allowed_dcp_frame_rates.begin(); i != allowed_dcp_frame_rates.end(); ++i) {
		candidates.push_back (FrameRateCandidate (float (*i) / 2, *i));
		candidates.push_back (FrameRateCandidate (float (*i) * 2, *i));
	}

	/* Pick the best one */
	float error = std::numeric_limits<float>::max ();
	optional<FrameRateCandidate> best;
	list<FrameRateCandidate>::iterator i = candidates.begin();
	while (i != candidates.end()) {

		float this_error = 0;
		for (ContentList::const_iterator j = _content.begin(); j != _content.end(); ++j) {
			shared_ptr<VideoContent> vc = dynamic_pointer_cast<VideoContent> (*j);
			if (!vc) {
				continue;
			}

			/* Use the largest difference between DCP and source as the "error" */
			this_error = max (this_error, float (fabs (i->source - vc->video_frame_rate ())));
		}

		if (this_error < error) {
			error = this_error;
			best = *i;
		}

		++i;
	}

	if (!best) {
		return 24;
	}
	
	return best->dcp;
}

Time
Playlist::length () const
{
	Time len = 0;
	for (ContentList::const_iterator i = _content.begin(); i != _content.end(); ++i) {
		len = max (len, (*i)->end ());
	}

	return len;
}

void
Playlist::reconnect ()
{
	for (list<boost::signals2::connection>::iterator i = _content_connections.begin(); i != _content_connections.end(); ++i) {
		i->disconnect ();
	}

	_content_connections.clear ();
		
	for (ContentList::iterator i = _content.begin(); i != _content.end(); ++i) {
		_content_connections.push_back ((*i)->Changed.connect (bind (&Playlist::content_changed, this, _1, _2, _3)));
	}
}

Time
Playlist::video_end () const
{
	Time end = 0;
	for (ContentList::const_iterator i = _content.begin(); i != _content.end(); ++i) {
		if (dynamic_pointer_cast<const VideoContent> (*i)) {
			end = max (end, (*i)->end ());
		}
	}

	return end;
}

void
Playlist::set_sequence_video (bool s)
{
	_sequence_video = s;
}

bool
ContentSorter::operator() (shared_ptr<Content> a, shared_ptr<Content> b)
{
	return a->start() < b->start();
}

/** @return content in an undefined order */
ContentList
Playlist::content () const
{
	return _content;
}

void
Playlist::repeat (ContentList c, int n)
{
	pair<Time, Time> range (TIME_MAX, 0);
	for (ContentList::iterator i = c.begin(); i != c.end(); ++i) {
		range.first = min (range.first, (*i)->start ());
		range.second = max (range.second, (*i)->start ());
		range.first = min (range.first, (*i)->end ());
		range.second = max (range.second, (*i)->end ());
	}

	Time pos = range.second;
	for (int i = 0; i < n; ++i) {
		for (ContentList::iterator i = c.begin(); i != c.end(); ++i) {
			shared_ptr<Content> copy = (*i)->clone ();
			copy->set_start (pos + copy->start() - range.first);
			_content.push_back (copy);
		}
		pos += range.second - range.first;
	}

	reconnect ();
	Changed ();
}
