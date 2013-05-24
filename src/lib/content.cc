/* -*- c-basic-offset: 8; default-tab-width: 8; -*- */

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

using std::string;
using boost::shared_ptr;
using boost::lexical_cast;

int const ContentProperty::START = 400;

Content::Content (shared_ptr<const Film> f, Time s)
	: _film (f)
	, _start (s)
{

}

Content::Content (shared_ptr<const Film> f, boost::filesystem::path p)
	: _film (f)
	, _file (p)
	, _start (0)
{

}

Content::Content (shared_ptr<const Film> f, shared_ptr<const cxml::Node> node)
	: _film (f)
{
	_file = node->string_child ("File");
	_digest = node->string_child ("Digest");
	_start = node->number_child<Time> ("Start");
}

Content::Content (Content const & o)
	: boost::enable_shared_from_this<Content> (o)
	, _film (o._film)
	, _file (o._file)
	, _digest (o._digest)
	, _start (o._start)
{

}

void
Content::as_xml (xmlpp::Node* node) const
{
	boost::mutex::scoped_lock lm (_mutex);
	node->add_child("File")->add_child_text (_file.string());
	node->add_child("Digest")->add_child_text (_digest);
	node->add_child("Start")->add_child_text (lexical_cast<string> (_start));
}

void
Content::examine (shared_ptr<Job>)
{
	string const d = md5_digest (_file);
	boost::mutex::scoped_lock lm (_mutex);
	_digest = d;
}

void
Content::signal_changed (int p)
{
	Changed (shared_from_this (), p);
}

void
Content::set_start (Time s)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_start = s;
	}

	signal_changed (ContentProperty::START);
}
