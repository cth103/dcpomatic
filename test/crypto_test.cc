/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#include "lib/crypto.h"
#include "lib/exceptions.h"
#include "test.h"
#include <openssl/rand.h>
#include <boost/test/unit_test.hpp>

using std::string;
using std::list;

BOOST_AUTO_TEST_CASE (crypto_test)
{
	dcp::Data key (dcpomatic::crypto_key_length());
	dcp::Data iv = dcpomatic::random_iv ();

	RAND_bytes (key.data().get(), dcpomatic::crypto_key_length());

	dcp::Data ciphertext = dcpomatic::encrypt ("Can you see any fish?", key, iv);
	BOOST_REQUIRE_EQUAL (dcpomatic::decrypt (ciphertext, key, iv), "Can you see any fish?");

	key.data()[5]++;
	BOOST_REQUIRE_THROW (dcpomatic::decrypt (ciphertext, key, iv), CryptoError);
}

