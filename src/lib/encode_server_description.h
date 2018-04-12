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
	{}

	/** @param h Server host name or IP address in string form.
	 *  @param t Number of threads to use on the server.
	 *  @param l Server link version number of the server.
	 */
	EncodeServerDescription (std::string h, int t, int l)
		: _host_name (h)
		, _threads (t)
		, _link_version (l)
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

	int link_version () const {
		return _link_version;
	}

	void set_host_name (std::string n) {
		_host_name = n;
	}

	void set_threads (int t) {
		_threads = t;
	}

private:
	/** server's host name */
	std::string _host_name;
	/** number of threads to use on the server */
	int _threads;
	/** server link (i.e. protocol) version number */
	int _link_version;
};

#endif
