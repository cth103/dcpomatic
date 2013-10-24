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
#include "content.h"
#include "util.h"
#include "content_factory.h"
#include "ui_signaller.h"

using std::string;
using std::stringstream;
using std::set;
using boost::shared_ptr;
using boost::lexical_cast;

int const ContentProperty::PATH = 400;
int const ContentProperty::POSITION = 401;
int const ContentProperty::LENGTH = 402;
int const ContentProperty::TRIM_START = 403;
int const ContentProperty::TRIM_END = 404;

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
	, _path (p)
	, _position (0)
	, _trim_start (0)
	, _trim_end (0)
	, _change_signals_frequent (false)
{

}

Content::Content (shared_ptr<const Film> f, shared_ptr<const cxml::Node> node)
	: _film (f)
	, _change_signals_frequent (false)
{
	_path = node->string_child ("Path");
	_digest = node->string_child ("Digest");
	_position = node->number_child<Time> ("Position");
	_trim_start = node->number_child<Time> ("TrimStart");
	_trim_end = node->number_child<Time> ("TrimEnd");
}

void
Content::as_xml (xmlpp::Node* node) const
{
	boost::mutex::scoped_lock lm (_mutex);
	
	node->add_child("Path")->add_child_text (_path.string());
	node->add_child("Digest")->add_child_text (_digest);
	node->add_child("Position")->add_child_text (lexical_cast<string> (_position));
	node->add_child("TrimStart")->add_child_text (lexical_cast<string> (_trim_start));
	node->add_child("TrimEnd")->add_child_text (lexical_cast<string> (_trim_end));
}

void
Content::examine (shared_ptr<Job> job)
{
	boost::mutex::scoped_lock lm (_mutex);
	boost::filesystem::path p = _path;
	lm.unlock ();
	
	string d;
	if (boost::filesystem::is_regular_file (p)) {
		d = md5_digest (p);
	} else {
		d = md5_digest_directory (p, job);
	}

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
	return content_factory (film, shared_ptr<cxml::Node> (new cxml::Node (node)));
}

string
Content::technical_summary () const
{
	return String::compose ("%1 %2 %3", path(), digest(), position());
}

Time
Content::length_after_trim () const
{
	return full_length() - trim_start() - trim_end();
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
	stringstream s;
	
	s << Content::digest()
	  << "_" << position()
	  << "_" << trim_start()
	  << "_" << trim_end();

	return s.str ();
}

bool
Content::path_valid () const
{
	return boost::filesystem::exists (_path);
}

void
Content::set_path (boost::filesystem::path path)
{
	_path = path;
	signal_changed (ContentProperty::PATH);
}

	
