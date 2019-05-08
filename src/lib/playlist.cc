/*
    Copyright (C) 2013-2018 Carl Hetherington <cth@carlh.net>

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

#include "playlist.h"
#include "video_content.h"
#include "text_content.h"
#include "ffmpeg_decoder.h"
#include "ffmpeg_content.h"
#include "image_decoder.h"
#include "audio_content.h"
#include "content_factory.h"
#include "dcp_content.h"
#include "job.h"
#include "config.h"
#include "util.h"
#include "digester.h"
#include "compose.hpp"
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
using namespace dcpomatic;

Playlist::Playlist ()
	: _sequence (true)
	, _sequencing (false)
{

}

Playlist::~Playlist ()
{
	boost::mutex::scoped_lock lm (_mutex);
	_content.clear ();
	disconnect ();
}

void
Playlist::content_change (weak_ptr<const Film> weak_film, ChangeType type, weak_ptr<Content> content, int property, bool frequent)
{
	/* Make sure we only hear about atomic changes (e.g. a PENDING always with the DONE/CANCELLED)
	   Ignore any DONE/CANCELLED that arrives without a PENDING.
	*/
	if (_checker.send (type, property)) {
		return;
	}

	shared_ptr<const Film> film = weak_film.lock ();
	DCPOMATIC_ASSERT (film);

	if (type == CHANGE_TYPE_DONE) {
		if (
			property == ContentProperty::TRIM_START ||
			property == ContentProperty::TRIM_END ||
			property == ContentProperty::LENGTH ||
			property == VideoContentProperty::FRAME_TYPE
			) {
			/* Don't respond to position changes here, as:
			   - sequencing after earlier/later changes is handled by move_earlier/move_later
			   - any other position changes will be timeline drags which should not result in content
			   being sequenced.
			*/
			maybe_sequence (film);
		}

		if (
			property == ContentProperty::POSITION ||
			property == ContentProperty::LENGTH ||
			property == ContentProperty::TRIM_START ||
			property == ContentProperty::TRIM_END) {

			bool changed = false;

			{
				boost::mutex::scoped_lock lm (_mutex);
				ContentList old = _content;
				sort (_content.begin(), _content.end(), ContentSorter ());
				changed = _content != old;
			}

			if (changed) {
				OrderChanged ();
			}
		}
	}

	ContentChange (type, content, property, frequent);
}

void
Playlist::maybe_sequence (shared_ptr<const Film> film)
{
	if (!_sequence || _sequencing) {
		return;
	}

	_sequencing = true;

	ContentList cont = content ();

	/* Keep track of the content that we've set the position of so that we don't
	   do it twice.
	*/
	ContentList placed;

	/* Video */

	DCPTime next_left;
	DCPTime next_right;
	BOOST_FOREACH (shared_ptr<Content> i, cont) {
		if (!i->video) {
			continue;
		}

		if (i->video->frame_type() == VIDEO_FRAME_TYPE_3D_RIGHT) {
			i->set_position (film, next_right);
			next_right = i->end(film);
		} else {
			i->set_position (film, next_left);
			next_left = i->end(film);
		}

		placed.push_back (i);
	}

	/* Captions */

	DCPTime next;
	BOOST_FOREACH (shared_ptr<Content> i, cont) {
		if (i->text.empty() || find (placed.begin(), placed.end(), i) != placed.end()) {
			continue;
		}

		i->set_position (film, next);
		next = i->end(film);
	}


	/* This won't change order, so it does not need a sort */

	_sequencing = false;
}

string
Playlist::video_identifier () const
{
	string t;

	BOOST_FOREACH (shared_ptr<const Content> i, content()) {
		bool burn = false;
		BOOST_FOREACH (shared_ptr<TextContent> j, i->text) {
			if (j->burn()) {
				burn = true;
			}
		}
		if (i->video || burn) {
			t += i->identifier ();
		}
	}

	Digester digester;
	digester.add (t.c_str(), t.length());
	return digester.get ();
}

/** @param film Film that this Playlist is for.
 *  @param node &lt;Playlist&gt; node.
 *  @param version Metadata version number.
 *  @param notes Output notes about that happened.
 */
void
Playlist::set_from_xml (shared_ptr<const Film> film, cxml::ConstNodePtr node, int version, list<string>& notes)
{
	boost::mutex::scoped_lock lm (_mutex);

	BOOST_FOREACH (cxml::NodePtr i, node->node_children ("Content")) {
		shared_ptr<Content> content = content_factory (i, version, notes);

		/* See if this content should be nudged to start on a video frame */
		DCPTime const old_pos = content->position();
		content->set_position(film, old_pos);
		if (old_pos != content->position()) {
			string note = _("Your project contains video content that was not aligned to a frame boundary.");
			note += "  ";
			if (old_pos < content->position()) {
				note += String::compose(
					_("The file %1 has been moved %2 milliseconds later."),
					content->path_summary(), DCPTime(content->position() - old_pos).seconds() * 1000
					);
			} else {
				note += String::compose(
					_("The file %1 has been moved %2 milliseconds earlier."),
					content->path_summary(), DCPTime(content->position() - old_pos).seconds() * 1000
					);
			}
			notes.push_back (note);
		}

		/* ...or have a start trim which is an integer number of frames */
		ContentTime const old_trim = content->trim_start();
		content->set_trim_start(old_trim);
		if (old_trim != content->trim_start()) {
			string note = _("Your project contains video content whose trim was not aligned to a frame boundary.");
			note += "  ";
			if (old_trim < content->trim_start()) {
				note += String::compose(
					_("The file %1 has been trimmed by %2 milliseconds more."),
					content->path_summary(), ContentTime(content->trim_start() - old_trim).seconds() * 1000
					);
			} else {
				note += String::compose(
					_("The file %1 has been trimmed by %2 milliseconds less."),
					content->path_summary(), ContentTime(old_trim - content->trim_start()).seconds() * 1000
					);
			}
			notes.push_back (note);
		}

		_content.push_back (content);
	}

	/* This shouldn't be necessary but better safe than sorry (there could be old files) */
	sort (_content.begin(), _content.end(), ContentSorter ());

	reconnect (film);
}

/** @param node &lt;Playlist&gt; node.
 *  @param with_content_paths true to include &lt;Path&gt; nodes in &lt;Content&gt; nodes, false to omit them.
 */
void
Playlist::as_xml (xmlpp::Node* node, bool with_content_paths)
{
	BOOST_FOREACH (shared_ptr<Content> i, content()) {
		i->as_xml (node->add_child ("Content"), with_content_paths);
	}
}

void
Playlist::add (shared_ptr<const Film> film, shared_ptr<Content> c)
{
	Change (CHANGE_TYPE_PENDING);

	{
		boost::mutex::scoped_lock lm (_mutex);
		_content.push_back (c);
		sort (_content.begin(), _content.end(), ContentSorter ());
		reconnect (film);
	}

	Change (CHANGE_TYPE_DONE);
}

void
Playlist::remove (shared_ptr<Content> c)
{
	Change (CHANGE_TYPE_PENDING);

	bool cancelled = false;

	{
		boost::mutex::scoped_lock lm (_mutex);

		ContentList::iterator i = _content.begin ();
		while (i != _content.end() && *i != c) {
			++i;
		}

		if (i != _content.end()) {
			_content.erase (i);
		} else {
			cancelled = true;
		}
	}

	if (cancelled) {
		Change (CHANGE_TYPE_CANCELLED);
	} else {
		Change (CHANGE_TYPE_DONE);
	}

	/* This won't change order, so it does not need a sort */
}

void
Playlist::remove (ContentList c)
{
	Change (CHANGE_TYPE_PENDING);

	{
		boost::mutex::scoped_lock lm (_mutex);

		BOOST_FOREACH (shared_ptr<Content> i, c) {
			ContentList::iterator j = _content.begin ();
			while (j != _content.end() && *j != i) {
				++j;
			}

			if (j != _content.end ()) {
				_content.erase (j);
			}
		}
	}

	/* This won't change order, so it does not need a sort */

	Change (CHANGE_TYPE_DONE);
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

/** @return the best frame rate from Config::_allowed_dcp_frame_rates for the content in this list */
int
Playlist::best_video_frame_rate () const
{
	list<int> const allowed_dcp_frame_rates = Config::instance()->allowed_dcp_frame_rates ();

	/* Work out what rates we could manage, including those achieved by using skip / repeat */
	list<FrameRateCandidate> candidates;

	/* Start with the ones without skip / repeat so they will get matched in preference to skipped/repeated ones */
	BOOST_FOREACH (int i, allowed_dcp_frame_rates) {
		candidates.push_back (FrameRateCandidate(i, i));
	}

	/* Then the skip/repeat ones */
	BOOST_FOREACH (int i, allowed_dcp_frame_rates) {
		candidates.push_back (FrameRateCandidate (float(i) / 2, i));
		candidates.push_back (FrameRateCandidate (float(i) * 2, i));
	}

	/* Pick the best one */
	float error = std::numeric_limits<float>::max ();
	optional<FrameRateCandidate> best;
	list<FrameRateCandidate>::iterator i = candidates.begin();
	while (i != candidates.end()) {

		float this_error = 0;
		BOOST_FOREACH (shared_ptr<Content> j, content()) {
			if (!j->video || !j->video_frame_rate()) {
				continue;
			}

			/* Best error for this content; we could use the content as-is or double its rate */
			float best_error = min (
				float (fabs (i->source - j->video_frame_rate().get())),
				float (fabs (i->source - j->video_frame_rate().get() * 2))
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
Playlist::length (shared_ptr<const Film> film) const
{
	DCPTime len;
	BOOST_FOREACH (shared_ptr<const Content> i, content()) {
		len = max (len, i->end(film));
	}

	return len;
}

/** @return position of the first thing on the playlist, if it's not empty */
optional<DCPTime>
Playlist::start () const
{
	ContentList cont = content ();
	if (cont.empty()) {
		return optional<DCPTime> ();
	}

	DCPTime start = DCPTime::max ();
	BOOST_FOREACH (shared_ptr<Content> i, cont) {
		start = min (start, i->position ());
	}

	return start;
}

/** Must be called with a lock held on _mutex */
void
Playlist::disconnect ()
{
	BOOST_FOREACH (boost::signals2::connection& i, _content_connections) {
		i.disconnect ();
	}

	_content_connections.clear ();
}

/** Must be called with a lock held on _mutex */
void
Playlist::reconnect (shared_ptr<const Film> film)
{
	disconnect ();

	BOOST_FOREACH (shared_ptr<Content> i, _content) {
		_content_connections.push_back (i->Change.connect(boost::bind(&Playlist::content_change, this, film, _1, _2, _3, _4)));
	}
}

DCPTime
Playlist::video_end (shared_ptr<const Film> film) const
{
	DCPTime end;
	BOOST_FOREACH (shared_ptr<Content> i, content()) {
		if (i->video) {
			end = max (end, i->end(film));
		}
	}

	return end;
}

DCPTime
Playlist::text_end (shared_ptr<const Film> film) const
{
	DCPTime end;
	BOOST_FOREACH (shared_ptr<Content> i, content()) {
		if (!i->text.empty ()) {
			end = max (end, i->end(film));
		}
	}

	return end;
}

FrameRateChange
Playlist::active_frame_rate_change (DCPTime t, int dcp_video_frame_rate) const
{
	ContentList cont = content ();
	for (ContentList::const_reverse_iterator i = cont.rbegin(); i != cont.rend(); ++i) {
		if (!(*i)->video) {
			continue;
		}

		if ((*i)->position() <= t) {
			/* This is the first piece of content (going backwards...) that starts before t,
			   so it's the active one.
			*/
			if ((*i)->video_frame_rate ()) {
				/* This content specified a rate, so use it */
				return FrameRateChange ((*i)->video_frame_rate().get(), dcp_video_frame_rate);
			} else {
				/* No specified rate so just use the DCP one */
				return FrameRateChange (dcp_video_frame_rate, dcp_video_frame_rate);
			}
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
	boost::mutex::scoped_lock lm (_mutex);
	return _content;
}

void
Playlist::repeat (shared_ptr<const Film> film, ContentList c, int n)
{
	pair<DCPTime, DCPTime> range (DCPTime::max (), DCPTime ());
	BOOST_FOREACH (shared_ptr<Content> i, c) {
		range.first = min (range.first, i->position ());
		range.second = max (range.second, i->position ());
		range.first = min (range.first, i->end(film));
		range.second = max (range.second, i->end(film));
	}

	Change (CHANGE_TYPE_PENDING);

	{
		boost::mutex::scoped_lock lm (_mutex);

		DCPTime pos = range.second;
		for (int i = 0; i < n; ++i) {
			BOOST_FOREACH (shared_ptr<Content> j, c) {
				shared_ptr<Content> copy = j->clone ();
				copy->set_position (film, pos + copy->position() - range.first);
				_content.push_back (copy);
			}
			pos += range.second - range.first;
		}

		sort (_content.begin(), _content.end(), ContentSorter ());
		reconnect (film);
	}

	Change (CHANGE_TYPE_DONE);
}

void
Playlist::move_earlier (shared_ptr<const Film> film, shared_ptr<Content> c)
{
	ContentList cont = content ();
	ContentList::iterator previous = cont.end();
	ContentList::iterator i = cont.begin();
	while (i != cont.end() && *i != c) {
		previous = i;
		++i;
	}

	DCPOMATIC_ASSERT (i != cont.end());
	if (previous == cont.end()) {
		return;
	}

	shared_ptr<Content> previous_c = *previous;

	DCPTime const p = previous_c->position ();
	previous_c->set_position (film, p + c->length_after_trim(film));
	c->set_position (film, p);
}

void
Playlist::move_later (shared_ptr<const Film> film, shared_ptr<Content> c)
{
	ContentList cont = content ();
	ContentList::iterator i = cont.begin();
	while (i != cont.end() && *i != c) {
		++i;
	}

	DCPOMATIC_ASSERT (i != cont.end());

	ContentList::iterator next = i;
	++next;

	if (next == cont.end()) {
		return;
	}

	shared_ptr<Content> next_c = *next;

	next_c->set_position (film, c->position());
	c->set_position (film, c->position() + next_c->length_after_trim(film));
}

int64_t
Playlist::required_disk_space (shared_ptr<const Film> film, int j2k_bandwidth, int audio_channels, int audio_frame_rate) const
{
	int64_t video = uint64_t (j2k_bandwidth / 8) * length(film).seconds();
	int64_t audio = uint64_t (audio_channels * audio_frame_rate * 3) * length(film).seconds();

	BOOST_FOREACH (shared_ptr<Content> i, content()) {
		shared_ptr<DCPContent> d = dynamic_pointer_cast<DCPContent> (i);
		if (d) {
			if (d->reference_video()) {
				video -= uint64_t (j2k_bandwidth / 8) * d->length_after_trim(film).seconds();
			}
			if (d->reference_audio()) {
				audio -= uint64_t (audio_channels * audio_frame_rate * 3) * d->length_after_trim(film).seconds();
			}
		}
	}

	/* Add on 64k for bits and pieces (metadata, subs etc) */
	return video + audio + 65536;
}

string
Playlist::content_summary (shared_ptr<const Film> film, DCPTimePeriod period) const
{
	string best_summary;
	int best_score = -1;
	BOOST_FOREACH (shared_ptr<Content> i, content()) {
		int score = 0;
		optional<DCPTimePeriod> const o = DCPTimePeriod(i->position(), i->end(film)).overlap (period);
		if (o) {
			score += 100 * o.get().duration().get() / period.duration().get();
		}

		if (i->video) {
			score += 100;
		}

		if (score > best_score) {
			best_summary = i->path(0).leaf().string();
			best_score = score;
		}
	}

	return best_summary;
}

pair<double, double>
Playlist::speed_up_range (int dcp_video_frame_rate) const
{
	pair<double, double> range (DBL_MAX, -DBL_MAX);

	BOOST_FOREACH (shared_ptr<Content> i, content()) {
		if (!i->video) {
			continue;
		}
		if (i->video_frame_rate()) {
			FrameRateChange const frc (i->video_frame_rate().get(), dcp_video_frame_rate);
			range.first = min (range.first, frc.speed_up);
			range.second = max (range.second, frc.speed_up);
		} else {
			FrameRateChange const frc (dcp_video_frame_rate, dcp_video_frame_rate);
			range.first = min (range.first, frc.speed_up);
			range.second = max (range.second, frc.speed_up);
		}
	}

	return range;
}
