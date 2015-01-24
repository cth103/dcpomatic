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

#include <boost/test/unit_test.hpp>
#include <dcp/cpl.h>
#include "lib/film.h"
#include "lib/dcp_subtitle_content.h"
#include "lib/ratio.h"
#include "lib/dcp_content_type.h"
#include "lib/dcp_content.h"
#include "lib/ffmpeg_content.h"
#include "lib/config.h"
#include "test.h"

using boost::shared_ptr;

/** Make an encrypted DCP, import it and make a new unencrypted DCP */
BOOST_AUTO_TEST_CASE (import_dcp_test)
{
	shared_ptr<Film> A = new_test_film ("import_dcp_test");
	A->set_container (Ratio::from_id ("185"));
	A->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	A->set_name ("frobozz");

	shared_ptr<FFmpegContent> c (new FFmpegContent (A, "test/data/test.mp4"));
	A->examine_and_add_content (c);
	A->set_encrypted (true);
	wait_for_jobs ();

	A->make_dcp ();
	wait_for_jobs ();

	dcp::DCP A_dcp ("build/test/import_dcp_test/" + A->dcp_name());
	A_dcp.read ();

	dcp::EncryptedKDM kdm = A->make_kdm (
		Config::instance()->decryption_certificate(),
		A_dcp.cpls().front()->file (),
		dcp::LocalTime ("2014-07-21T00:00:00+00:00"),
		dcp::LocalTime ("2024-07-21T00:00:00+00:00"),
		dcp::MODIFIED_TRANSITIONAL_1
		);

	shared_ptr<Film> B = new_test_film ("import_dcp_test2");
	B->set_container (Ratio::from_id ("185"));
	B->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	B->set_name ("frobozz");

	shared_ptr<DCPContent> d (new DCPContent (B, "build/test/import_dcp_test/" + A->dcp_name()));
	d->add_kdm (kdm);
	B->examine_and_add_content (d);
	wait_for_jobs ();

	B->make_dcp ();
	wait_for_jobs ();

	check_dcp ("build/test/import_dcp_test2/" + B->dcp_name(), "test/data/import_dcp_test2");
}
