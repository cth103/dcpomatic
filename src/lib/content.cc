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

#include <boost/thread/mutex.hpp>
#include <libxml++/libxml++.h>
#include <libcxml/cxml.h>
#include <libdcp/raw_convert.h>
#include "content.h"
#include "util.h"
#include "content_factory.h"
#include "ui_signaller.h"
#include "exceptions.h"
#include "film.h"
#include "safe_stringstream.h"

#include "i18n.h"

using std::string;
using std::set;
using std::list;
using std::cout;
using std::vector;
using std::max;
using boost::shared_ptr;
using libdcp::raw_convert;

int const ContentProperty::PATH = 400;
int const ContentProperty::POSITION = 401;
int const ContentProperty::LENGTH = 402;
int const ContentProperty::TRIM_START = 403;
int const ContentProperty::TRIM_END = 404;

Content::Content (shared_ptr<const Film> f)
	: _film (f)
	, _position (0)
	, _trim_start (0)
	, _trim_end (0)
	, _change_signals_frequent (false)
{

}

Content::Content (shared_ptr<const Film> f, Time p)
	: _film (f)
	, _position (p)
	, _trim_start (0)
	, _trim_end (0)
	, _change_signals_frequent (false)
{

}

Content::Content (shared_ptr<const Film> f, boost::filesystem::path p)
	: _film (f)
	, _position (0)
	, _trim_start (0)
	, _trim_end (0)
	, _change_signals_frequent (false)
{
	_paths.push_back (p);
}

Content::Content (shared_ptr<const Film> f, shared_ptr<const cxml::Node> node)
	: _film (f)
	, _change_signals_frequent (false)
{
	list<cxml::NodePtr> path_children = node->node_children ("Path");
	for (list<cxml::NodePtr>::const_iterator i = path_children.begin(); i != path_children.end(); ++i) {
		_paths.push_back ((*i)->content ());
	}
	_digest = node->string_child ("Digest");
	_position = node->number_child<Time> ("Position");
	_trim_start = node->number_child<Time> ("TrimStart");
	_trim_end = node->number_child<Time> ("TrimEnd");
}

Content::Content (shared_ptr<const Film> f, vector<shared_ptr<Content> > c)
	: _film (f)
	, _position (c.front()->position ())
	, _trim_start (c.front()->trim_start ())
	, _trim_end (c.back()->trim_end ())
	, _change_signals_frequent (false)
{
	for (size_t i = 0; i < c.size(); ++i) {
		if (i > 0 && c[i]->trim_start ()) {
			throw JoinError (_("Only the first piece of content to be joined can have a start trim."));
		}

		if (i < (c.size() - 1) && c[i]->trim_end ()) {
			throw JoinError (_("Only the last piece of content to be joined can have an end trim."));
		}

		for (size_t j = 0; j < c[i]->number_of_paths(); ++j) {
			_paths.push_back (c[i]->path (j));
		}
	}
}

void
Content::as_xml (xmlpp::Node* node) const
{
	boost::mutex::scoped_lock lm (_mutex);

	for (vector<boost::filesystem::path>::const_iterator i = _paths.begin(); i != _paths.end(); ++i) {
		node->add_child("Path")->add_child_text (i->string ());
	}
	node->add_child("Digest")->add_child_text (_digest);
	node->add_child("Position")->add_child_text (raw_convert<string> (_position));
	node->add_child("TrimStart")->add_child_text (raw_convert<string> (_trim_start));
	node->add_child("TrimEnd")->add_child_text (raw_convert<string> (_trim_end));
}

void
Content::examine (shared_ptr<Job> job)
{
	boost::mutex::scoped_lock lm (_mutex);
	vector<boost::filesystem::path> p = _paths;
	lm.unlock ();
	
	string const d = md5_digest (p, job);

	lm.lock ();
	_digest = d;
}

void
Content::signal_changed (int p)
{
	if (ui_signaller) {
		ui_signaller->emit (boost::bind (boost::ref (Changed), shared_from_this (), p, _change_signals_frequent));
	}
}

void
Content::set_position (Time p)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		if (p == _position) {
			return;
		}
		
		_position = p;
	}

	signal_changed (ContentProperty::POSITION);
}

void
Content::set_trim_start (Time t)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_trim_start = t;
	}

	signal_changed (ContentProperty::TRIM_START);
}

void
Content::set_trim_end (Time t)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_trim_end = t;
	}

	signal_changed (ContentProperty::TRIM_END);
}


shared_ptr<Content>
Content::clone () const
{
	shared_ptr<const Film> film = _film.lock ();
	if (!film) {
		return shared_ptr<Content> ();
	}
	
	/* This is a bit naughty, but I can't think of a compelling reason not to do it ... */
	xmlpp::Document doc;
	xmlpp::Node* node = doc.create_root_node ("Content");
	as_xml (node);

	/* notes is unused here (we assume) */
	list<string> notes;
	return content_factory (film, cxml::NodePtr (new cxml::Node (node)), Film::current_state_version, notes);
}

string
Content::technical_summary () const
{
	return String::compose ("%1 %2 %3", path_summary(), digest(), position());
}

Time
Content::length_after_trim () const
{
	return max (int64_t (0), full_length() - trim_start() - trim_end());
}

/** @param t A time relative to the start of this content (not the position).
 *  @return true if this time is trimmed by our trim settings.
 */
bool
Content::trimmed (Time t) const
{
	return (t < trim_start() || t > (full_length() - trim_end ()));
}

/** @return string which includes everything about how this content affects
 *  its playlist.
 */
string
Content::identifier () const
{
	SafeStringStream s;
	
	s << Content::digest()
	  << "_" << position()
	  << "_" << trim_start()
	  << "_" << trim_end();

	return s.str ();
}

bool
Content::paths_valid () const
{
	for (vector<boost::filesystem::path>::const_iterator i = _paths.begin(); i != _paths.end(); ++i) {
		if (!boost::filesystem::exists (*i)) {
			return false;
		}
	}

	return true;
}

void
Content::set_path (boost::filesystem::path path)
{
	_paths.clear ();
	_paths.push_back (path);
	signal_changed (ContentProperty::PATH);
}

string
Content::path_summary () const
{
	/* XXX: should handle multiple paths more gracefully */

	assert (number_of_paths ());

	string s = path(0).filename().string ();
	if (number_of_paths() > 1) {
		s += " ...";
	}

	return s;
}
