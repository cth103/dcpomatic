/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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

#include "lib/content_factory.h"
#include "lib/film.h"
#include "lib/dcp_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>

using boost::shared_ptr;
using boost::dynamic_pointer_cast;

void check_within_n (int64_t a, int64_t b, int64_t n)
{
	BOOST_CHECK (abs (a - b) <= n);
}


BOOST_AUTO_TEST_CASE (required_disk_space_test)
{
	shared_ptr<Film> film = new_test_film ("required_disk_space_test");
	film->set_j2k_bandwidth (100000000);
	film->set_audio_channels (6);
	shared_ptr<Content> content_a = content_factory (film, "test/data/flat_blue.png");
	film->examine_and_add_content (content_a);
	shared_ptr<DCPContent> content_b = dynamic_pointer_cast<DCPContent> (content_factory (film, "test/data/burnt_subtitle_test_dcp"));
	film->examine_and_add_content (content_b);
	wait_for_jobs ();
	film->write_metadata ();

	check_within_n (
		film->required_disk_space(),
		289LL * (100000000 / 8) / 24 +  // video
		289LL * 48000 * 6 * 3 / 24 +    // audio
		65536,                          // extra
		16
		);

	content_b->set_reference_video (true);

	check_within_n (
		film->required_disk_space(),
		240LL * (100000000 / 8) / 24 +  // video
		289LL * 48000 * 6 * 3 / 24 +    // audio
		65536,                          // extra
		16
		);

	content_b->set_reference_audio (true);

	check_within_n (
		film->required_disk_space(),
		240LL * (100000000 / 8) / 24 +  // video
		240LL * 48000 * 6 * 3 / 24 +    // audio
		65536,                          // extra
		16
		);
}
