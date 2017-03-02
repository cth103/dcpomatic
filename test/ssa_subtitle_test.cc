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

/** @file  test/ssa_subtitle_test.cc
 *  @brief Test use of SSA subtitle files.
 *  @ingroup specific
 */

#include "lib/film.h"
#include "lib/text_subtitle_content.h"
#include "lib/dcp_content_type.h"
#include "lib/font.h"
#include "lib/ratio.h"
#include "lib/subtitle_content.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string.hpp>
#include <list>

using std::string;
using std::list;
using boost::shared_ptr;

/** Make a DCP with subs from a .ssa file */
BOOST_AUTO_TEST_CASE (ssa_subtitle_test1)
{
	shared_ptr<Film> film = new_test_film ("ssa_subtitle_test1");

	film->set_container (Ratio::from_id ("185"));
	film->set_dcp_content_type (DCPContentType::from_isdcf_name ("TLR"));
	film->set_name ("frobozz");
	film->set_interop (true);
	shared_ptr<TextSubtitleContent> content (new TextSubtitleContent (film, private_data / "DKH_UT_EN20160601def.ssa"));
	film->examine_and_add_content (content);
	wait_for_jobs ();

	content->subtitle->set_use (true);
	content->subtitle->set_burn (false);

	film->make_dcp ();
	wait_for_jobs ();

	/* Find the subtitle file and check it */
	for (
		boost::filesystem::directory_iterator i = boost::filesystem::directory_iterator (film->directory().get() / film->dcp_name (false));
		i != boost::filesystem::directory_iterator ();
		++i) {

		if (boost::filesystem::is_directory (i->path ())) {
			for (
				boost::filesystem::directory_iterator j = boost::filesystem::directory_iterator (i->path ());
				j != boost::filesystem::directory_iterator ();
				++j) {

				if (boost::algorithm::starts_with (j->path().leaf().string(), "sub_")) {
					list<string> ignore;
					ignore.push_back ("SubtitleID");
					check_xml (*j, private_data / "DKH_UT_EN20160601def.xml", ignore);
				}
			}
		}
	}
}
