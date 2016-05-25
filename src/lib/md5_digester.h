/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include <openssl/md5.h>
#include <boost/noncopyable.hpp>
#include <boost/optional.hpp>
#include <string>

class MD5Digester : public boost::noncopyable
{
public:
	MD5Digester ();
	~MD5Digester ();

	void add (void const * data, size_t size);

	template <class T>
	void add (T data) {
		add (&data, sizeof (T));
	}

	void add (std::string const & s);

	std::string get () const;

private:
	mutable MD5_CTX _context;
	mutable boost::optional<std::string> _digest;
};
