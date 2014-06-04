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

#include <iomanip>
#include <sstream>
#include <openssl/md5.h>
#include "md5_digester.h"

using std::string;
using std::stringstream;
using std::hex;
using std::setfill;
using std::setw;

MD5Digester::MD5Digester ()
{
	MD5_Init (&_context);
}

MD5Digester::~MD5Digester ()
{
	get ();
}

void
MD5Digester::add (void const * data, size_t size)
{
	MD5_Update (&_context, data, size);
}

string
MD5Digester::get () const
{
	if (!_digest) {
		unsigned char digest[MD5_DIGEST_LENGTH];
		MD5_Final (digest, &_context);
		
		stringstream s;
		for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
			s << hex << setfill('0') << setw(2) << ((int) digest[i]);
		}
		
		_digest = s.str ();
	}
	
	return _digest.get ();
}
