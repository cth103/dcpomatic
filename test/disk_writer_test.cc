/*
    Copyright (C) 2020-2021 Carl Hetherington <cth@carlh.net>

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


#include "lib/cross.h"
#include "lib/ext.h"
#include "test.h"
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/filesystem.hpp>
#include <boost/process.hpp>
#include <boost/test/unit_test.hpp>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <iostream>
#include <future>
#include <regex>


using std::future;
using std::string;
using std::vector;


static
void
create_empty (boost::filesystem::path file, int size)
{
	auto fd = open (file.string().c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	BOOST_REQUIRE (fd != -1);
	auto const r = posix_fallocate (fd, 0, size);
	BOOST_REQUIRE_EQUAL (r, 0);
	close (fd);
}


vector<string>
ext2_ls (vector<string> arguments)
{
	using namespace boost::process;

	boost::asio::io_service ios;
	future<string> data;
	child ch (search_path("e2ls"), arguments, std_in.close(), std_out > data, ios);
	ios.run();

	auto output = data.get();
	boost::trim (output);
	vector<string> parts;
	boost::split (parts, output, boost::is_any_of("\t "), boost::token_compress_on);
	return parts;
}


/** Use the writer code to make a disk and partition and copy a file (in a directory)
 *  to it, then check that:
 *  - the partition has inode size 128
 *  - the file and directory have reasonable timestamps
 *  - the file can be copied back off the disk
 */
BOOST_AUTO_TEST_CASE (disk_writer_test1)
{
	using namespace boost::filesystem;
	using namespace boost::process;

	remove_all ("build/test/disk_writer_test1.disk");
	remove_all ("build/test/disk_writer_test1.partition");
	remove_all ("build/test/disk_writer_test1");

	path disk = "build/test/disk_writer_test1.disk";
	path partition = "build/test/disk_writer_test1.partition";

	/* lwext4 has a lower limit of correct ext2 partition sizes it can make; 32Mb
	 * does not work here: fsck gives errors about an incorrect free blocks count.
	 */
	create_empty (disk, 256 * 1024 * 1024);
	create_empty (partition, 256 * 1024 * 1024);

	path dcp = "build/test/disk_writer_test1";
	create_directory (dcp);
	/* Some arbitrary file size here */
	make_random_file (dcp / "foo", 1024 * 1024 * 32 - 6128);

	dcpomatic::write (dcp, disk.string(), partition.string(), 0);

	BOOST_CHECK_EQUAL (system("/sbin/e2fsck -fn build/test/disk_writer_test1.partition"), 0);

	{
		boost::asio::io_service ios;
		future<string> data;
		child ch ("/sbin/tune2fs", args({"-l", partition.string()}), std_in.close(), std_out > data, ios);
		ios.run();

		string output = data.get();
		std::smatch matches;
		std::regex reg("Inode size:\\s*(.*)");
		BOOST_REQUIRE (std::regex_search(output, matches, reg));
		BOOST_REQUIRE (matches.size() == 2);
		BOOST_CHECK_EQUAL (matches[1].str(), "128");
	}

	BOOST_CHECK (ext2_ls({partition.string()}) == vector<string>({"disk_writer_test1", "lost+found"}));

	string const unset_date = "1-Jan-1970";

	/* Check timestamp of the directory has been set */
	auto details = ext2_ls({"-l", partition.string()});
	BOOST_REQUIRE (details.size() >= 6);
	BOOST_CHECK (details[5] != unset_date);

	auto const dir = partition.string() + ":disk_writer_test1";
	BOOST_CHECK (ext2_ls({dir}) == vector<string>({"foo"}));

	/* Check timestamp of foo */
	details = ext2_ls({"-l", dir});
	BOOST_REQUIRE (details.size() >= 6);
	BOOST_CHECK (details[5] != unset_date);

	system ("e2cp " + partition.string() + ":disk_writer_test1/foo build/test/disk_writer_test1_foo_back");
	check_file ("build/test/disk_writer_test1/foo", "build/test/disk_writer_test1_foo_back");
}

