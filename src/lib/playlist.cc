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

#include "playlist.h"
#include "sndfile_content.h"
#include "sndfile_decoder.h"
#include "video_content.h"
#include "subtitle_content.h"
#include "ffmpeg_decoder.h"
#include "ffmpeg_content.h"
#include "image_decoder.h"
#include "content_factory.h"
#include "dcp_content.h"
#include "job.h"
#include "config.h"
#include "util.h"
#include "md5_digester.h"
#include <libcxml/cxml.h>
#include <libxml++/libxml++.h>
#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>
#include <iostream>

#include "i18n.h"

using std::list;
using std::cout;
using std::vector;
using std::min;
using std::max;
using std::string;
using std::pair;
using boost::optional;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;

Playlist::Playlist ()
	: _sequence (true)
	, _sequencing (false)
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
	if (property == ContentProperty::LENGTH || property == VideoContentProperty::VIDEO_FRAME_TYPE) {
		/* Don't respond to position changes here, as:
		   - sequencing after earlier/later changes is handled by move_earlier/move_later
		   - any other position changes will be timeline drags which should not result in content
		   being sequenced.
		*/
		maybe_sequence ();
	}

	if (
		property == ContentProperty::POSITION ||
		property == ContentProperty::LENGTH ||
		property == ContentProperty::TRIM_START ||
		property == ContentProperty::TRIM_END) {

		ContentList old = _content;
		sort (_content.begin(), _content.end(), ContentSorter ());
		if (_content != old) {
			OrderChanged ();
		}
	}

	ContentChanged (content, property, frequent);
}

void
Playlist::maybe_sequence ()
{
	if (!_sequence || _sequencing) {
		return;
	}

	_sequencing = true;

	/* Keep track of the content that we've set the position of so that we don't
	   do it twice.
	*/
	ContentList placed;

	/* Video */

	DCPTime next_left;
	DCPTime next_right;
	BOOST_FOREACH (shared_ptr<Content> i, _content) {
		if (!i->video) {
			continue;
		}

		if (i->video->video_frame_type() == VIDEO_FRAME_TYPE_3D_RIGHT) {
			i->set_position (next_right);
			next_right = i->end();
		} else {
			i->set_position (next_left);
			next_left = i->end();
		}

		placed.push_back (i);
	}

	/* Subtitles */

	DCPTime next;
	BOOST_FOREACH (shared_ptr<Content> i, _content) {
		if (!i->subtitle || find (placed.begin(), placed.end(), i) != placed.end()) {
			continue;
		}

		i->set_position (next);
		next = i->end();
	}


	/* This won't change order, so it does not need a sort */

	_sequencing = false;
}

string
Playlist::video_identifier () const
{
	string t;

	BOOST_FOREACH (shared_ptr<const Content> i, _content) {
		shared_ptr<const VideoContent> vc = dynamic_pointer_cast<const VideoContent> (i);
		shared_ptr<const SubtitleContent> sc = dynamic_pointer_cast<const SubtitleContent> (i);
		if (vc) {
			t += vc->identifier ();
		} else if (sc && sc->burn_subtitles ()) {
			t += sc->identifier ();
		}
	}

	MD5Digester digester;
	digester.add (t.c_str(), t.length());
	return digester.get ();
}

/** @param node <Playlist> node */
void
Playlist::set_from_xml (shared_ptr<const Film> film, cxml::ConstNodePtr node, int version, list<string>& notes)
{
	BOOST_FOREACH (cxml::NodePtr i, node->node_children ("Content")) {
		_content.push_back (content_factory (film, i, version, notes));
	}

	/* This shouldn't be necessary but better safe than sorry (there could be old files) */
	sort (_content.begin(), _content.end(), ContentSorter ());

	reconnect ();
}

/** @param node <Playlist> node */
void
Playlist::as_xml (xmlpp::Node* node)
{
	BOOST_FOREACH (shared_ptr<Content> i, _content) {
		i->as_xml (node->add_child ("Content"));
	}
}

void
Playlist::add (shared_ptr<Content> c)
{
	_content.push_back (c);
	sort (_content.begin(), _content.end(), ContentSorter ());
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

	/* This won't change order, so it does not need a sort */
}

void
Playlist::remove (ContentList c)
{
	BOOST_FOREACH (shared_ptr<Content> i, c) {
		ContentList::iterator j = _content.begin ();
		while (j != _content.end() && *j != i) {
			++j;
		}

		if (j != _content.end ()) {
			_content.erase (j);
		}
	}

	/* This won't change order, so it does not need a sort */

	Changed ();
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

	/* Work out what rates we could manage, including those achieved by using skip / repeat */
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
		BOOST_FOREACH (shared_ptr<Content> j, _content) {
			if (!j->video || !j->video->has_own_video_frame_rate()) {
				continue;
			}

			/* Best error for this content; we could use the content as-is or double its rate */
			float best_error = min (
				float (fabs (i->source - j->video->video_frame_rate ())),
				float (fabs (i->source - j->video->video_frame_rate () * 2))
				);

			/* Use the largest difference between DCP and source as the "error" */
			this_error = max (this_error, best_error);
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

/** @return length of the playlist from time 0 to the last thing on the playlist */
DCPTime
Playlist::length () const
{
	DCPTime len;
	BOOST_FOREACH (shared_ptr<const Content> i, _content) {
		len = max (len, i->end());
	}

	return len;
}

/** @return position of the first thing on the playlist, if it's not empty */
optional<DCPTime>
Playlist::start () const
{
	if (_content.empty ()) {
		return optional<DCPTime> ();
	}

	DCPTime start = DCPTime::max ();
	BOOST_FOREACH (shared_ptr<Content> i, _content) {
		start = min (start, i->position ());
	}

	return start;
}

void
Playlist::reconnect ()
{
	for (list<boost::signals2::connection>::iterator i = _content_connections.begin(); i != _content_connections.end(); ++i) {
		i->disconnect ();
	}

	_content_connections.clear ();

	BOOST_FOREACH (shared_ptr<Content> i, _content) {
		_content_connections.push_back (i->Changed.connect (bind (&Playlist::content_changed, this, _1, _2, _3)));
	}
}

DCPTime
Playlist::video_end () const
{
	DCPTime end;
	BOOST_FOREACH (shared_ptr<Content> i, _content) {
		if (dynamic_pointer_cast<const VideoContent> (i)) {
			end = max (end, i->end ());
		}
	}

	return end;
}

DCPTime
Playlist::subtitle_end () const
{
	DCPTime end;
	BOOST_FOREACH (shared_ptr<Content> i, _content) {
		if (dynamic_pointer_cast<const SubtitleContent> (i)) {
			end = max (end, i->end ());
		}
	}

	return end;
}

FrameRateChange
Playlist::active_frame_rate_change (DCPTime t, int dcp_video_frame_rate) const
{
	for (ContentList::const_reverse_iterator i = _content.rbegin(); i != _content.rend(); ++i) {
		if (!(*i)->video) {
			continue;
		}

		if ((*i)->position() <= t) {
			/* This is the first piece of content (going backwards...) that starts before t,
			   so it's the active one.
			*/
			return FrameRateChange ((*i)->video->video_frame_rate(), dcp_video_frame_rate);
		}
	}

	return FrameRateChange (dcp_video_frame_rate, dcp_video_frame_rate);
}

void
Playlist::set_sequence (bool s)
{
	_sequence = s;
}

bool
ContentSorter::operator() (shared_ptr<Content> a, shared_ptr<Content> b)
{
	if (a->position() != b->position()) {
		return a->position() < b->position();
	}

	/* Put video before audio if they start at the same time */
	if (a->video && !b->video) {
		return true;
	} else if (!a->video && b->video) {
		return false;
	}

	/* Last resort */
	return a->digest() < b->digest();
}

/** @return content in ascending order of position */
ContentList
Playlist::content () const
{
	return _content;
}

void
Playlist::repeat (ContentList c, int n)
{
	pair<DCPTime, DCPTime> range (DCPTime::max (), DCPTime ());
	BOOST_FOREACH (shared_ptr<Content> i, c) {
		range.first = min (range.first, i->position ());
		range.second = max (range.second, i->position ());
		range.first = min (range.first, i->end ());
		range.second = max (range.second, i->end ());
	}

	DCPTime pos = range.second;
	for (int i = 0; i < n; ++i) {
		BOOST_FOREACH (shared_ptr<Content> j, c) {
			shared_ptr<Content> copy = j->clone ();
			copy->set_position (pos + copy->position() - range.first);
			_content.push_back (copy);
		}
		pos += range.second - range.first;
	}

	sort (_content.begin(), _content.end(), ContentSorter ());

	reconnect ();
	Changed ();
}

void
Playlist::move_earlier (shared_ptr<Content> c)
{
	ContentList::iterator previous = _content.end ();
	ContentList::iterator i = _content.begin();
	while (i != _content.end() && *i != c) {
		previous = i;
		++i;
	}

	DCPOMATIC_ASSERT (i != _content.end ());
	if (previous == _content.end ()) {
		return;
	}

	shared_ptr<Content> previous_c = *previous;

	DCPTime const p = previous_c->position ();
	previous_c->set_position (p + c->length_after_trim ());
	c->set_position (p);
}

void
Playlist::move_later (shared_ptr<Content> c)
{
	ContentList::iterator i = _content.begin();
	while (i != _content.end() && *i != c) {
		++i;
	}

	DCPOMATIC_ASSERT (i != _content.end ());

	ContentList::iterator next = i;
	++next;

	if (next == _content.end ()) {
		return;
	}

	shared_ptr<Content> next_c = *next;

	next_c->set_position (c->position ());
	c->set_position (c->position() + next_c->length_after_trim ());
}

int64_t
Playlist::required_disk_space (int j2k_bandwidth, int audio_channels, int audio_frame_rate) const
{
	int64_t video = uint64_t (j2k_bandwidth / 8) * length().seconds ();
	int64_t audio = uint64_t (audio_channels * audio_frame_rate * 3) * length().seconds ();

	BOOST_FOREACH (shared_ptr<Content> i, _content) {
		shared_ptr<DCPContent> d = dynamic_pointer_cast<DCPContent> (i);
		if (d) {
			if (d->reference_video()) {
				video -= uint64_t (j2k_bandwidth / 8) * d->length_after_trim().seconds();
			}
			if (d->reference_audio()) {
				audio -= uint64_t (audio_channels * audio_frame_rate * 3) * d->length_after_trim().seconds();
			}
		}
	}

	/* Add on 64k for bits and pieces (metadata, subs etc) */
	return video + audio + 65536;
}
