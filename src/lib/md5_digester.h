/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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
	
	std::string get () const;

private:
	mutable MD5_CTX _context;
	mutable boost::optional<std::string> _digest;
};
