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

#ifndef DCPOMATIC_CONTENT_H
#define DCPOMATIC_CONTENT_H

#include <set>
#include <boost/filesystem.hpp>
#include <boost/signals2.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <libxml++/libxml++.h>
#include "types.h"

namespace cxml {
	class Node;
}

class Job;
class Film;

class ContentProperty
{
public:
	static int const PATH;
	static int const POSITION;
	static int const LENGTH;
	static int const TRIM_START;
	static int const TRIM_END;
};

class Content : public boost::enable_shared_from_this<Content>, public boost::noncopyable
{
public:
	Content (boost::shared_ptr<const Film>);
	Content (boost::shared_ptr<const Film>, Time);
	Content (boost::shared_ptr<const Film>, boost::filesystem::path);
	Content (boost::shared_ptr<const Film>, boost::shared_ptr<const cxml::Node>);
	Content (boost::shared_ptr<const Film>, std::vector<boost::shared_ptr<Content> >);
	virtual ~Content () {}
	
	virtual void examine (boost::shared_ptr<Job>);
	virtual std::string summary () const = 0;
	/** @return Technical details of this content; these are written to logs to
	 *  help with debugging.
	 */
	virtual std::string technical_summary () const;
	virtual std::string information () const = 0;
	virtual void as_xml (xmlpp::Node *) const;
	virtual Time full_length () const = 0;
	virtual std::string identifier () const;

	boost::shared_ptr<Content> clone () const;

	void set_path (boost::filesystem::path);

	std::string path_summary () const;

	std::vector<boost::filesystem::path> paths () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _paths;
	}

	size_t number_of_paths () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _paths.size ();
	}

	boost::filesystem::path path (size_t i) const {
		boost::mutex::scoped_lock lm (_mutex);
		return _paths[i];
	}
	
	bool paths_valid () const;

	/** @return MD5 digest of the content's file(s) */
	std::string digest () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _digest;
	}

	void set_position (Time);

	/** Time that this content starts; i.e. the time that the first
	 *  bit of the content (trimmed or not) will happen.
	 */
	Time position () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _position;
	}

	void set_trim_start (Time);

	Time trim_start () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _trim_start;
	}

	void set_trim_end (Time);
	
	Time trim_end () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _trim_end;
	}
	
	Time end () const {
		return position() + length_after_trim() - 1;
	}

	Time length_after_trim () const;
	
	void set_change_signals_frequent (bool f) {
		_change_signals_frequent = f;
	}

	bool trimmed (Time) const;

	boost::signals2::signal<void (boost::weak_ptr<Content>, int, bool)> Changed;

protected:
	void signal_changed (int);

	boost::weak_ptr<const Film> _film;

	/** _mutex which should be used to protect accesses, as examine
	    jobs can update content state in threads other than the main one.
	*/
	mutable boost::mutex _mutex;

	/** Paths of our data files */
	std::vector<boost::filesystem::path> _paths;
	
private:
	std::string _digest;
	Time _position;
	Time _trim_start;
	Time _trim_end;
	bool _change_signals_frequent;
};

#endif
