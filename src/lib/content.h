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

#ifndef DVDOMATIC_CONTENT_H
#define DVDOMATIC_CONTENT_H

#include <boost/filesystem.hpp>
#include <boost/signals2.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <libxml++/libxml++.h>

namespace cxml {
	class Node;
}

class Job;
class Film;

class Content : public boost::enable_shared_from_this<Content>
{
public:
	Content (boost::filesystem::path);
	Content (boost::shared_ptr<const cxml::Node>);
	Content (Content const &);
	
	virtual void examine (boost::shared_ptr<Film>, boost::shared_ptr<Job>, bool);
	virtual std::string summary () const = 0;
	virtual std::string information () const = 0;
	virtual void as_xml (xmlpp::Node *) const;
	virtual boost::shared_ptr<Content> clone () const = 0;
	
	boost::filesystem::path file () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _file;
	}

	std::string digest () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _digest;
	}

	boost::signals2::signal<void (boost::weak_ptr<Content>, int)> Changed;

protected:
	void signal_changed (int);
	
	mutable boost::mutex _mutex;

private:
	boost::filesystem::path _file;
	std::string _digest;
};

#endif
