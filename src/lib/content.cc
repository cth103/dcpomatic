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

/** @file  src/lib/content.cc
 *  @brief Content class.
 */

#include "content.h"
#include "change_signaller.h"
#include "util.h"
#include "content_factory.h"
#include "video_content.h"
#include "audio_content.h"
#include "text_content.h"
#include "exceptions.h"
#include "film.h"
#include "job.h"
#include "compose.hpp"
#include <dcp/locale_convert.h>
#include <dcp/raw_convert.h>
#include <libcxml/cxml.h>
#include <libxml++/libxml++.h>
#include <boost/thread/mutex.hpp>
#include <iostream>

#include "i18n.h"

using std::string;
using std::list;
using std::cout;
using std::vector;
using std::max;
using std::pair;
using boost::shared_ptr;
using boost::optional;
using dcp::raw_convert;
using dcp::locale_convert;

int const ContentProperty::PATH = 400;
int const ContentProperty::POSITION = 401;
int const ContentProperty::LENGTH = 402;
int const ContentProperty::TRIM_START = 403;
int const ContentProperty::TRIM_END = 404;
int const ContentProperty::VIDEO_FRAME_RATE = 405;

Content::Content ()
	: _position (0)
	, _trim_start (0)
	, _trim_end (0)
	, _change_signals_frequent (false)
{

}

Content::Content (DCPTime p)
	: _position (p)
	, _trim_start (0)
	, _trim_end (0)
	, _change_signals_frequent (false)
{

}

Content::Content (boost::filesystem::path p)
	: _position (0)
	, _trim_start (0)
	, _trim_end (0)
	, _change_signals_frequent (false)
{
	add_path (p);
}

Content::Content (cxml::ConstNodePtr node)
	: _change_signals_frequent (false)
{
	list<cxml::NodePtr> path_children = node->node_children ("Path");
	BOOST_FOREACH (cxml::NodePtr i, path_children) {
		_paths.push_back (i->content());
		optional<time_t> const mod = i->optional_number_attribute<time_t>("mtime");
		if (mod) {
			_last_write_times.push_back (*mod);
		} else if (boost::filesystem::exists(i->content())) {
			_last_write_times.push_back (boost::filesystem::last_write_time(i->content()));
		} else {
			_last_write_times.push_back (0);
		}
	}
	_digest = node->optional_string_child ("Digest").get_value_or ("X");
	_position = DCPTime (node->number_child<DCPTime::Type> ("Position"));
	_trim_start = ContentTime (node->number_child<ContentTime::Type> ("TrimStart"));
	_trim_end = ContentTime (node->number_child<ContentTime::Type> ("TrimEnd"));
	_video_frame_rate = node->optional_number_child<double> ("VideoFrameRate");
}

Content::Content (vector<shared_ptr<Content> > c)
	: _position (c.front()->position ())
	, _trim_start (c.front()->trim_start ())
	, _trim_end (c.back()->trim_end ())
	, _video_frame_rate (c.front()->video_frame_rate())
	, _change_signals_frequent (false)
{
	for (size_t i = 0; i < c.size(); ++i) {
		if (i > 0 && c[i]->trim_start() > ContentTime ()) {
			throw JoinError (_("Only the first piece of content to be joined can have a start trim."));
		}

		if (i < (c.size() - 1) && c[i]->trim_end () > ContentTime ()) {
			throw JoinError (_("Only the last piece of content to be joined can have an end trim."));
		}

		if (
			(_video_frame_rate && !c[i]->_video_frame_rate) ||
			(!_video_frame_rate && c[i]->_video_frame_rate)
			) {
			throw JoinError (_("Content to be joined must have the same video frame rate"));
		}

		if (_video_frame_rate && fabs (_video_frame_rate.get() - c[i]->_video_frame_rate.get()) > VIDEO_FRAME_RATE_EPSILON) {
			throw JoinError (_("Content to be joined must have the same video frame rate"));
		}

		for (size_t j = 0; j < c[i]->number_of_paths(); ++j) {
			_paths.push_back (c[i]->path(j));
			_last_write_times.push_back (c[i]->_last_write_times[j]);
		}
	}
}

void
Content::as_xml (xmlpp::Node* node, bool with_paths) const
{
	boost::mutex::scoped_lock lm (_mutex);

	if (with_paths) {
		for (size_t i = 0; i < _paths.size(); ++i) {
			xmlpp::Element* p = node->add_child("Path");
			p->add_child_text (_paths[i].string());
			p->set_attribute ("mtime", raw_convert<string>(_last_write_times[i]));
		}
	}
	node->add_child("Digest")->add_child_text (_digest);
	node->add_child("Position")->add_child_text (raw_convert<string> (_position.get ()));
	node->add_child("TrimStart")->add_child_text (raw_convert<string> (_trim_start.get ()));
	node->add_child("TrimEnd")->add_child_text (raw_convert<string> (_trim_end.get ()));
	if (_video_frame_rate) {
		node->add_child("VideoFrameRate")->add_child_text (raw_convert<string> (_video_frame_rate.get()));
	}
}

string
Content::calculate_digest () const
{
	boost::mutex::scoped_lock lm (_mutex);
	vector<boost::filesystem::path> p = _paths;
	lm.unlock ();

	/* Some content files are very big, so we use a poor man's
	   digest here: a digest of the first and last 1e6 bytes with the
	   size of the first file tacked on the end as a string.
	*/
	return digest_head_tail(p, 1000000) + raw_convert<string>(boost::filesystem::file_size(p.front()));
}

void
Content::examine (shared_ptr<const Film>, shared_ptr<Job> job)
{
	if (job) {
		job->sub (_("Computing digest"));
	}

	string const d = calculate_digest ();

	boost::mutex::scoped_lock lm (_mutex);
	_digest = d;

	_last_write_times.clear ();
	BOOST_FOREACH (boost::filesystem::path i, _paths) {
		_last_write_times.push_back (boost::filesystem::last_write_time(i));
	}
}

void
Content::signal_change (ChangeType c, int p)
{
	try {
		if (c == CHANGE_TYPE_PENDING || c == CHANGE_TYPE_CANCELLED) {
			Change (c, shared_from_this(), p, _change_signals_frequent);
		} else {
			emit (boost::bind (boost::ref(Change), c, shared_from_this(), p, _change_signals_frequent));
		}
	} catch (boost::bad_weak_ptr &) {
		/* This must be during construction; never mind */
	}
}

void
Content::set_position (shared_ptr<const Film> film, DCPTime p, bool force_emit)
{
	/* video and audio content can modify its position */

	if (video) {
		video->modify_position (film, p);
	}

	/* Only allow the audio to modify if we have no video;
	   sometimes p can't be on an integer video AND audio frame,
	   and in these cases we want the video constraint to be
	   satisfied since (I think) the audio code is better able to
	   cope.
	*/
	if (!video && audio) {
		audio->modify_position (film, p);
	}

	ChangeSignaller<Content> cc (this, ContentProperty::POSITION);

	{
		boost::mutex::scoped_lock lm (_mutex);
		if (p == _position && !force_emit) {
			cc.abort ();
			return;
		}

		_position = p;
	}
}

void
Content::set_trim_start (ContentTime t)
{
	/* video and audio content can modify its start trim */

	if (video) {
		video->modify_trim_start (t);
	}

	/* See note in ::set_position */
	if (!video && audio) {
		audio->modify_trim_start (t);
	}

	ChangeSignaller<Content> cc (this, ContentProperty::TRIM_START);

	{
		boost::mutex::scoped_lock lm (_mutex);
		_trim_start = t;
	}
}

void
Content::set_trim_end (ContentTime t)
{
	ChangeSignaller<Content> cc (this, ContentProperty::TRIM_END);

	{
		boost::mutex::scoped_lock lm (_mutex);
		_trim_end = t;
	}
}


shared_ptr<Content>
Content::clone () const
{
	/* This is a bit naughty, but I can't think of a compelling reason not to do it ... */
	xmlpp::Document doc;
	xmlpp::Node* node = doc.create_root_node ("Content");
	as_xml (node, true);

	/* notes is unused here (we assume) */
	list<string> notes;
	return content_factory (cxml::NodePtr(new cxml::Node(node)), Film::current_state_version, notes);
}

string
Content::technical_summary () const
{
	string s = String::compose ("%1 %2 %3", path_summary(), digest(), position().seconds());
	if (_video_frame_rate) {
		s += String::compose(" %1", *_video_frame_rate);
	}
	return s;
}

DCPTime
Content::length_after_trim (shared_ptr<const Film> film) const
{
	return max (DCPTime(), full_length(film) - DCPTime(trim_start() + trim_end(), film->active_frame_rate_change(position())));
}

/** @return string which changes when something about this content changes which affects
 *  the appearance of its video.
 */
string
Content::identifier () const
{
	char buffer[256];
	snprintf (
		buffer, sizeof(buffer), "%s_%" PRId64 "_%" PRId64 "_%" PRId64,
		Content::digest().c_str(), position().get(), trim_start().get(), trim_end().get()
		);
	return buffer;
}

bool
Content::paths_valid () const
{
	BOOST_FOREACH (boost::filesystem::path i, _paths) {
		if (!boost::filesystem::exists (i)) {
			return false;
		}
	}

	return true;
}

void
Content::set_paths (vector<boost::filesystem::path> paths)
{
	ChangeSignaller<Content> cc (this, ContentProperty::PATH);

	{
		boost::mutex::scoped_lock lm (_mutex);
		_paths = paths;
		_last_write_times.clear ();
		BOOST_FOREACH (boost::filesystem::path i, _paths) {
			_last_write_times.push_back (boost::filesystem::last_write_time(i));
		}
	}
}

string
Content::path_summary () const
{
	/* XXX: should handle multiple paths more gracefully */

	DCPOMATIC_ASSERT (number_of_paths ());

	string s = path(0).filename().string ();
	if (number_of_paths() > 1) {
		s += " ...";
	}

	return s;
}

/** @return a list of properties that might be interesting to the user */
list<UserProperty>
Content::user_properties (shared_ptr<const Film> film) const
{
	list<UserProperty> p;
	add_properties (film, p);
	return p;
}

/** @return DCP times of points within this content where a reel split could occur */
list<DCPTime>
Content::reel_split_points (shared_ptr<const Film>) const
{
	list<DCPTime> t;
	/* This is only called for video content and such content has its position forced
	   to start on a frame boundary.
	*/
	t.push_back (position());
	return t;
}

void
Content::set_video_frame_rate (double r)
{
	ChangeSignaller<Content> cc (this, ContentProperty::VIDEO_FRAME_RATE);

	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_video_frame_rate && fabs(r - *_video_frame_rate) < VIDEO_FRAME_RATE_EPSILON) {
			cc.abort();
		}
		_video_frame_rate = r;
	}

	/* Make sure trim is still on a frame boundary */
	if (video) {
		set_trim_start (trim_start());
	}
}

void
Content::unset_video_frame_rate ()
{
	ChangeSignaller<Content> cc (this, ContentProperty::VIDEO_FRAME_RATE);

	{
		boost::mutex::scoped_lock lm (_mutex);
		_video_frame_rate = optional<double>();
	}
}

double
Content::active_video_frame_rate (shared_ptr<const Film> film) const
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (_video_frame_rate) {
			return _video_frame_rate.get ();
		}
	}

	/* No frame rate specified, so assume this content has been
	   prepared for any concurrent video content or perhaps
	   just the DCP rate.
	*/
	return film->active_frame_rate_change(position()).source;
}

void
Content::add_properties (shared_ptr<const Film>, list<UserProperty>& p) const
{
	p.push_back (UserProperty (UserProperty::GENERAL, _("Filename"), path(0).string ()));

	if (_video_frame_rate) {
		if (video) {
			p.push_back (
				UserProperty (
					UserProperty::VIDEO,
					_("Frame rate"),
					locale_convert<string> (_video_frame_rate.get(), 5),
					_("frames per second")
					)
				);
		} else {
			p.push_back (
				UserProperty (
					UserProperty::GENERAL,
					_("Prepared for video frame rate"),
					locale_convert<string> (_video_frame_rate.get(), 5),
					_("frames per second")
					)
				);
		}
	}
}

/** Take settings from the given content if it is of the correct type */
void
Content::take_settings_from (shared_ptr<const Content> c)
{
	if (video && c->video) {
		video->take_settings_from (c->video);
	}
	if (audio && c->audio) {
		audio->take_settings_from (c->audio);
	}

	list<shared_ptr<TextContent> >::iterator i = text.begin ();
	list<shared_ptr<TextContent> >::const_iterator j = c->text.begin ();
	while (i != text.end() && j != c->text.end()) {
		(*i)->take_settings_from (*j);
		++i;
		++j;
 	}
}

shared_ptr<TextContent>
Content::only_text () const
{
	DCPOMATIC_ASSERT (text.size() < 2);
	if (text.empty ()) {
		return shared_ptr<TextContent> ();
	}
	return text.front ();
}

shared_ptr<TextContent>
Content::text_of_original_type (TextType type) const
{
	BOOST_FOREACH (shared_ptr<TextContent> i, text) {
		if (i->original_type() == type) {
			return i;
		}
	}

	return shared_ptr<TextContent> ();
}

void
Content::add_path (boost::filesystem::path p)
{
	boost::mutex::scoped_lock lm (_mutex);
	_paths.push_back (p);
	_last_write_times.push_back (boost::filesystem::last_write_time(p));
}
