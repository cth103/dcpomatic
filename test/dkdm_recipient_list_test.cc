/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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


#include "lib/config.h"
#include "lib/dkdm_recipient.h"
#include "lib/dkdm_recipient_list.h"
#include "test.h"
#include <dcp/filesystem.h>
#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_CASE(dkdm_receipient_list_copy_from_xml_test)
{
	ConfigRestorer cr("build/test/dkdm_recipient_list_copy_config");

	dcp::filesystem::remove_all(*Config::override_path);
	dcp::filesystem::create_directories(*Config::override_path);
	dcp::filesystem::copy_file("test/data/dkdm_recipients.xml", *Config::override_path / "dkdm_recipients.xml");

	DKDMRecipientList dkdm_recipient_list;
	dkdm_recipient_list.read_legacy_file(Config::instance()->read_path("dkdm_recipients.xml"));
	auto dkdm_recipients = dkdm_recipient_list.dkdm_recipients();
	BOOST_REQUIRE_EQUAL(dkdm_recipients.size(), 2U);

	auto dkdm_recipient_iter = dkdm_recipients.begin();
	BOOST_CHECK_EQUAL(dkdm_recipient_iter->second.name, "Bob's Epics");
	BOOST_CHECK_EQUAL(dkdm_recipient_iter->second.emails.size(), 2U);
	BOOST_CHECK_EQUAL(dkdm_recipient_iter->second.emails[0], "epicbob@gmail.com");
	BOOST_CHECK_EQUAL(dkdm_recipient_iter->second.emails[1], "boblikesemlong@cinema-bob.com");
	BOOST_CHECK_EQUAL(dkdm_recipient_iter->second.recipient->subject_dn_qualifier(), "r5/Q5f3UTm7qzoF5QzNZP6aEuvI=");
	++dkdm_recipient_iter;

	BOOST_CHECK_EQUAL(dkdm_recipient_iter->second.name, "Sharon's Shorts");
	BOOST_CHECK_EQUAL(dkdm_recipient_iter->second.notes, "Even if it sucks, at least it's over quickly");
	BOOST_CHECK_EQUAL(dkdm_recipient_iter->second.recipient->subject_dn_qualifier(), "FHerM3Us/DWuqD1MnztStSlFJO0=");
	++dkdm_recipient_iter;
}


