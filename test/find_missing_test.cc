/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#include "lib/content.h"
#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/film.h"
#include "lib/find_missing.h"
#include "test.h"
#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::shared_ptr;
using std::string;


BOOST_AUTO_TEST_CASE (find_missing_test_with_single_files)
{
	using namespace boost::filesystem;

	auto name = string{"find_missing_test_with_single_files"};

	/* Make a directory with some content */
	auto content_dir = path("build/test") / path(name + "_content");
	remove_all (content_dir);
	create_directories (content_dir);
	copy_file ("test/data/flat_red.png", content_dir / "A.png");
	copy_file ("test/data/flat_red.png", content_dir / "B.png");
	copy_file ("test/data/flat_red.png", content_dir / "C.png");

	/* Make a film with that content */
	auto film = new_test_film(name + "_film", {
		content_factory(content_dir / "A.png")[0],
		content_factory(content_dir / "B.png")[0],
		content_factory(content_dir / "C.png")[0]
		});
	film->write_metadata ();

	/* Move the content somewhere else */
	auto moved = path("build/test") / path(name + "_moved");
	remove_all (moved);
	rename (content_dir, moved);

	/* That should make the content paths invalid */
	for (auto content: film->content()) {
		BOOST_CHECK(!paths_exist(content->paths()));
	}

	/* Fix the missing files and check the result */
	dcpomatic::find_missing (film->content(), moved / "A.png");

	for (auto content: film->content()) {
		BOOST_CHECK(paths_exist(content->paths()));
	}
}


BOOST_AUTO_TEST_CASE (find_missing_test_with_multiple_files)
{
	using namespace boost::filesystem;

	auto name = string{"find_missing_test_with_multiple_files"};

	/* Copy an arbitrary DCP into a test directory */
	auto content_dir = path("build/test") / path(name + "_content");
	remove_all (content_dir);
	create_directories (content_dir);
	for (auto ref: directory_iterator("test/data/scaling_test_133_185")) {
		copy (ref, content_dir / ref.path().filename());
	}

	/* Make a film containing that DCP */
	auto film = new_test_film(name + "_film", { make_shared<DCPContent>(content_dir) });
	film->write_metadata ();

	/* Move the DCP's content elsewhere */
	auto moved = path("build/test") / path(name + "_moved");
	remove_all (moved);
	rename (content_dir, moved);

	/* That should make the content paths invalid */
	for (auto content: film->content()) {
		BOOST_CHECK(!paths_exist(content->paths()));
	}

	/* Fix the missing files and check the result */
	dcpomatic::find_missing (film->content(), moved / "foo");

	for (auto content: film->content()) {
		BOOST_CHECK(paths_exist(content->paths()));
	}
}


BOOST_AUTO_TEST_CASE (find_missing_test_with_multiple_files_one_incorrect)
{
	using namespace boost::filesystem;

	auto name = string{"find_missing_test_with_multiple_files_one_incorrect"};

	/* Copy an arbitrary DCP into a test directory */
	auto content_dir = path("build/test") / path(name + "_content");
	remove_all (content_dir);
	create_directories (content_dir);
	for (auto ref: directory_iterator("test/data/scaling_test_133_185")) {
		copy (ref, content_dir / ref.path().filename());
	}

	/* Make a film containing that DCP */
	auto film = new_test_film(name + "_film", { make_shared<DCPContent>(content_dir) });
	film->write_metadata ();

	/* Move the DCP's content elsewhere */
	auto moved = path("build/test") / path(name + "_moved");
	remove_all (moved);
	rename (content_dir, moved);

	/* Corrupt one of the files in the moved content, so that it should not be found in the find_missing
	 * step
	 */
	auto cpl = find_file(moved, "cpl_");
	remove (cpl);
	copy ("test/data/scaling_test_133_185/ASSETMAP.xml", cpl);

	/* The film's contents should be invalid */
	for (auto content: film->content()) {
		BOOST_CHECK(!paths_exist(content->paths()));
	}

	dcpomatic::find_missing (film->content(), moved / "foo");

	/* And even after find_missing there should still be missing content */
	for (auto content: film->content()) {
		BOOST_CHECK(!paths_exist(content->paths()));
	}
}


BOOST_AUTO_TEST_CASE(find_missing_test_with_rename)
{
	using namespace boost::filesystem;

	auto name = string{"find_missing_test_with_rename"};

	/* Make a directory with some content */
	auto content_dir = path("build/test") / path(name + "_content");
	remove_all(content_dir);
	create_directories(content_dir);
	copy_file("test/data/flat_red.png", content_dir / "A.png");
	copy_file("test/data/flat_red.png", content_dir / "B.png");
	copy_file("test/data/flat_red.png", content_dir / "C.png");

	/* Make a film with that content */
	auto film = new_test_film(name + "_film", {
		content_factory(content_dir / "A.png")[0],
		content_factory(content_dir / "B.png")[0],
		content_factory(content_dir / "C.png")[0]
		});
	film->write_metadata();

	/* Rename one of the files */
	rename(content_dir / "C.png", content_dir / "bogus.png");

	/* That should make one of the content paths invalid */
	auto content_list = film->content();
	int const valid = std::count_if(content_list.begin(), content_list.end(), [](shared_ptr<const Content> content) {
		return paths_exist(content->paths());
	});
	BOOST_CHECK_EQUAL(valid, 2);

	/* Fix the missing files and check the result */
	dcpomatic::find_missing(content_list, content_dir / "bogus.png");

	for (auto content: content_list) {
		BOOST_CHECK(paths_exist(content->paths()));
	}

}


BOOST_AUTO_TEST_CASE(test_film_saved_on_windows)
{
	auto film = make_shared<Film>(boost::filesystem::path("test/data/windows_film"));
	film->read_metadata();
	dcpomatic::find_missing(film->content(), TestPaths::private_data());

	for (auto content: film->content()) {
		BOOST_CHECK(paths_exist(content->paths()));
	}
}


BOOST_AUTO_TEST_CASE(test_film_saved_on_posix)
{
	auto film = make_shared<Film>(boost::filesystem::path("test/data/posix_film"));
	film->read_metadata();
	dcpomatic::find_missing(film->content(), TestPaths::private_data());

	for (auto content: film->content()) {
		BOOST_CHECK(paths_exist(content->paths()));
	}
}
