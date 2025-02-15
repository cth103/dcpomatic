/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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


#include "lib/email.h"
#include "smtp_server.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>


auto constexpr port = 31925;


BOOST_AUTO_TEST_CASE(email_retry_test)
{
	boost::thread thread([]() {
		run_smtp_server(port, true);
		run_smtp_server(port, true);
		run_smtp_server(port, false);
	});

	Email email("carl@crunchcinema.com", { "bob@snacks.com" }, "Louder crisps - possible?", "These crisps just aren't loud enough.  People can still hear the film.");
	email.send_with_retry("localhost", port, EmailProtocol::PLAIN, 3);
}

