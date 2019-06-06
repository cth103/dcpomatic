/*
    Copyright (C) 2019 Carl Hetherington <cth@carlh.net>

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

#include "lib/decrypted_ecinema_kdm.h"
#include "lib/encrypted_ecinema_kdm.h"
#include "lib/config.h"
#include <boost/test/unit_test.hpp>
extern "C" {
#include <libavutil/aes_ctr.h>
}
#include <fstream>

using std::string;
using boost::optional;

#ifdef DCPOMATIC_VARIANT_SWAROOP

BOOST_AUTO_TEST_CASE (ecinema_kdm_roundtrip_test1)
{
	dcp::Key key (AES_CTR_KEY_SIZE);
	DecryptedECinemaKDM dec ("123-456-789-0", "Hello world", key, optional<dcp::LocalTime>(), optional<dcp::LocalTime>());
	EncryptedECinemaKDM enc = dec.encrypt (Config::instance()->decryption_chain()->leaf());
	DecryptedECinemaKDM dec2 (enc, *Config::instance()->decryption_chain()->key());
	BOOST_CHECK_EQUAL (dec2.id(), "123-456-789-0");
	BOOST_CHECK_EQUAL (dec2.name(), "Hello world");
	BOOST_CHECK (dec2.key() == key);
	BOOST_CHECK (!static_cast<bool>(dec2.not_valid_before()));
	BOOST_CHECK (!static_cast<bool>(dec2.not_valid_after()));
}

BOOST_AUTO_TEST_CASE (ecinema_kdm_roundtrip_test2)
{
	dcp::Key key (AES_CTR_KEY_SIZE);
	DecryptedECinemaKDM dec ("123-456-789-0", "Hello world", key, dcp::LocalTime("2019-06-01T15:05:23+01:00"), dcp::LocalTime("2019-07-02T19:10:12+02:00"));
	EncryptedECinemaKDM enc = dec.encrypt (Config::instance()->decryption_chain()->leaf());
	DecryptedECinemaKDM dec2 (enc, *Config::instance()->decryption_chain()->key());
	BOOST_CHECK_EQUAL (dec2.id(), "123-456-789-0");
	BOOST_CHECK_EQUAL (dec2.name(), "Hello world");
	BOOST_CHECK (dec2.key() == key);
	BOOST_REQUIRE (static_cast<bool>(dec2.not_valid_before()));
	BOOST_CHECK_EQUAL (dec2.not_valid_before()->as_string(), "2019-06-01T15:05:23+01:00");
	BOOST_REQUIRE (static_cast<bool>(dec2.not_valid_after()));
	BOOST_CHECK_EQUAL (dec2.not_valid_after()->as_string(), "2019-07-02T19:10:12+02:00");
}

BOOST_AUTO_TEST_CASE (ecinema_kdm_roundtrip_test3)
{
	dcp::Key key (AES_CTR_KEY_SIZE);
	DecryptedECinemaKDM dec ("123-456-789-0", "Hello world", key, optional<dcp::LocalTime>(), optional<dcp::LocalTime>());
	EncryptedECinemaKDM enc = dec.encrypt (Config::instance()->decryption_chain()->leaf());
	string const enc_xml = enc.as_xml ();
	EncryptedECinemaKDM enc2 (enc_xml);
	DecryptedECinemaKDM dec2 (enc2, *Config::instance()->decryption_chain()->key());
	BOOST_CHECK_EQUAL (dec2.id(), "123-456-789-0");
	BOOST_CHECK_EQUAL (dec2.name(), "Hello world");
	BOOST_CHECK (dec2.key() == key);
	BOOST_CHECK (!static_cast<bool>(dec2.not_valid_before()));
	BOOST_CHECK (!static_cast<bool>(dec2.not_valid_after()));
}

BOOST_AUTO_TEST_CASE (ecinema_kdm_roundtrip_test4)
{
	dcp::Key key (AES_CTR_KEY_SIZE);
	DecryptedECinemaKDM dec ("123-456-789-0", "Hello world", key, dcp::LocalTime("2019-06-01T15:05:23+01:00"), dcp::LocalTime("2019-07-02T19:10:12+02:00"));
	EncryptedECinemaKDM enc = dec.encrypt (Config::instance()->decryption_chain()->leaf());
	string const enc_xml = enc.as_xml();
	EncryptedECinemaKDM enc2 (dcp::file_to_string("build/test/shit_the_bed.xml"));
	DecryptedECinemaKDM dec2 (enc2, *Config::instance()->decryption_chain()->key());
	BOOST_CHECK_EQUAL (dec2.id(), "123-456-789-0");
	BOOST_CHECK_EQUAL (dec2.name(), "Hello world");
	BOOST_CHECK (dec2.key() == key);
	BOOST_REQUIRE (static_cast<bool>(dec2.not_valid_before()));
	BOOST_CHECK_EQUAL (dec2.not_valid_before()->as_string(), "2019-06-01T15:05:23+01:00");
	BOOST_REQUIRE (static_cast<bool>(dec2.not_valid_after()));
	BOOST_CHECK_EQUAL (dec2.not_valid_after()->as_string(), "2019-07-02T19:10:12+02:00");
}

#endif
