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

#include "test.h"
#include "lib/film.h"
#include "lib/content_factory.h"
#include "lib/content.h"
#include "lib/video_content.h"
#include <boost/test/unit_test.hpp>

using std::string;
using std::list;
using boost::shared_ptr;

BOOST_AUTO_TEST_CASE (image_content_fade_test)
{
	shared_ptr<Film> film = new_test_film2 ("image_content_fade_test");
	shared_ptr<Content> content = content_factory("test/data/flat_red.png").front();
	film->examine_and_add_content (content);
	wait_for_jobs ();

	content->video->set_fade_in (1);
	film->make_dcp ();
	wait_for_jobs ();

	check_dcp ("test/data/image_content_fade_test", film->dir(film->dcp_name()));
}
