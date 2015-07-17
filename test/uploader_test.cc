/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "lib/uploader.h"
#include <boost/bind.hpp>
#include <boost/test/unit_test.hpp>
#include <fstream>

using std::vector;
using std::make_pair;
using std::pair;
using std::string;
using std::ofstream;
using boost::shared_ptr;
using boost::bind;

static void
set_status (string)
{

}

static void
set_progress (float)
{

}

class TestUploader : public Uploader
{
public:
	TestUploader ()
		: Uploader (bind (&set_status, _1), bind (&set_progress, _1))
	{
		_directories.push_back ("uploader");
		_directories.push_back ("uploader/c");
		_directories.push_back ("uploader/b");
		_directories.push_back ("uploader/b/e");
		_directories.push_back ("uploader/a");
		_next_directory = _directories.begin ();

		_files.push_back (make_pair ("test/data/uploader/b/e/f", "uploader/b/e/f"));
		_files.push_back (make_pair ("test/data/uploader/a/d", "uploader/a/d"));
		_next_file = _files.begin ();
	}

protected:
	void create_directory (boost::filesystem::path directory) {
		std::cout << directory << "\n";
		BOOST_CHECK (directory == *_next_directory);
		++_next_directory;
	}

	void upload_file (boost::filesystem::path from, boost::filesystem::path to, boost::uintmax_t &, boost::uintmax_t) {
		BOOST_CHECK (make_pair (from, to) == *_next_file);
		++_next_file;
	}

private:
	vector<boost::filesystem::path> _directories;
	vector<boost::filesystem::path>::iterator _next_directory;
	vector<std::pair<boost::filesystem::path, boost::filesystem::path> > _files;
	vector<std::pair<boost::filesystem::path, boost::filesystem::path> >::iterator _next_file;
};

BOOST_AUTO_TEST_CASE (uploader_test)
{
	boost::filesystem::remove_all ("test/data/uploader");
	boost::filesystem::create_directories ("test/data/uploader/a");
	boost::filesystem::create_directories ("test/data/uploader/b");
	boost::filesystem::create_directories ("test/data/uploader/c");
	boost::filesystem::create_directories ("test/data/uploader/b/e");

	ofstream f ("test/data/uploader/a/d");
	f.close ();

	ofstream g ("test/data/uploader/b/e/f");
	g.close ();

	TestUploader uploader;
	uploader.upload ("test/data/uploader");
}
