/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_ENCODE_SERVER_DESCRIPTION_H
#define DCPOMATIC_ENCODE_SERVER_DESCRIPTION_H

#include "types.h"
#include <boost/date_time/posix_time/posix_time.hpp>

/** @class EncodeServerDescription
 *  @brief Class to describe a server to which we can send encoding work.
 */
class EncodeServerDescription
{
public:
	EncodeServerDescription ()
		: _host_name ("")
		, _threads (1)
		, _link_version (0)
		, _last_seen (boost::posix_time::second_clock::local_time())
	{}

	/** @param h Server host name or IP address in string form.
	 *  @param t Number of threads to use on the server.
	 *  @param l Server link version number of the server.
	 */
	EncodeServerDescription (std::string h, int t, int l)
		: _host_name (h)
		, _threads (t)
		, _link_version (l)
		, _last_seen (boost::posix_time::second_clock::local_time())
	{}

	/* Default copy constructor is fine */

	/** @return server's host name or IP address in string form */
	std::string host_name () const {
		return _host_name;
	}

	/** @return number of threads to use on the server */
	int threads () const {
		return _threads;
	}

	bool current_link_version () const {
		return _link_version == SERVER_LINK_VERSION;
	}

	void set_host_name (std::string n) {
		_host_name = n;
	}

	void set_threads (int t) {
		_threads = t;
	}

	void set_seen () {
		_last_seen = boost::posix_time::second_clock::local_time();
	}

	int last_seen_seconds () const {
		return boost::posix_time::time_duration(boost::posix_time::second_clock::local_time() - _last_seen).total_seconds();
	}

private:
	/** server's host name */
	std::string _host_name;
	/** number of threads to use on the server */
	int _threads;
	/** server link (i.e. protocol) version number */
	int _link_version;
	boost::posix_time::ptime _last_seen;
};

#endif
