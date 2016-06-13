/*
    Copyright (C) 2014-2016 Carl Hetherington <cth@carlh.net>

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

#include "digester.h"
#include "safe_stringstream.h"
#include <nettle/md5.h>
#include <iomanip>

using std::string;
using std::hex;
using std::setfill;
using std::setw;

Digester::Digester ()
{
	md5_init (&_context);
}

Digester::~Digester ()
{
	get ();
}

void
Digester::add (void const * data, size_t size)
{
	md5_update (&_context, size, reinterpret_cast<uint8_t const *> (data));
}

void
Digester::add (string const & s)
{
	add (s.c_str(), s.length());
}

string
Digester::get () const
{
	if (!_digest) {
		unsigned char digest[MD5_DIGEST_SIZE];
		md5_digest (&_context, MD5_DIGEST_SIZE, digest);

		SafeStringStream s;
		for (int i = 0; i < MD5_DIGEST_SIZE; ++i) {
			s << hex << setfill('0') << setw(2) << ((int) digest[i]);
		}

		_digest = s.str ();
	}

	return _digest.get ();
}
