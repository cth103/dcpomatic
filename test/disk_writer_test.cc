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


static
void
make_empty_file(boost::filesystem::path file, off_t size)
{
       auto fd = open (file.string().c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
       BOOST_REQUIRE (fd != -1);
       auto const r = posix_fallocate (fd, 0, size);
       BOOST_REQUIRE_EQUAL (r, 0);
       close (fd);
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

	Cleanup cl;

	path disk = "build/test/disk_writer_test1.disk";
	path partition = "build/test/disk_writer_test1.partition";

	cl.add(disk);
	cl.add(partition);

	/* lwext4 has a lower limit of correct ext2 partition sizes it can make; 32Mb
	 * does not work here: fsck gives errors about an incorrect free blocks count.
	 */
	make_random_file(disk, 256 * 1024 * 1024);
	make_random_file(partition, 256 * 1024 * 1024);

	path dcp = "build/test/disk_writer_test1";
	create_directory (dcp);
	/* Some arbitrary file size here */
	make_random_file (dcp / "foo", 1024 * 1024 * 32 - 6128);

	dcpomatic::write ({dcp}, disk.string(), partition.string(), nullptr);

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

	cl.run();
}


BOOST_AUTO_TEST_CASE (disk_writer_test2)
{
	using namespace boost::filesystem;
	using namespace boost::process;

	remove_all("build/test/disk_writer_test2.disk");
	remove_all("build/test/disk_writer_test2.partition");
	remove_all("build/test/disk_writer_test2");

	Cleanup cl;

	path const disk = "build/test/disk_writer_test2.disk";
	path const partition = "build/test/disk_writer_test2.partition";

	cl.add(disk);
	cl.add(partition);

	/* Using empty files here still triggers the bug and is much quicker than using random data */
	make_empty_file(disk,      31043616768LL);
	make_empty_file(partition, 31043571712LL);

	auto const dcp = TestPaths::private_data() / "xm";
	dcpomatic::write({dcp}, disk.string(), partition.string(), nullptr);

	BOOST_CHECK_EQUAL(system("/sbin/e2fsck -fn build/test/disk_writer_test2.partition"), 0);

	path const check = "build/test/disk_writer_test2";
	create_directory(check);
	cl.add(check);

	for (auto original: directory_iterator(dcp)) {
		auto path_in_copy = path("xm") / original.path().filename();
		auto path_in_check = check / original.path().filename();
		system("e2cp " + partition.string() + ":" + path_in_copy.string() + " " + path_in_check.string());
		check_file(original.path(), path_in_check);
	}

	cl.run();
}



BOOST_AUTO_TEST_CASE (disk_writer_test3)
{
	using namespace boost::filesystem;
	using namespace boost::process;

	remove_all("build/test/disk_writer_test3.disk");
	remove_all("build/test/disk_writer_test3.partition");
	remove_all("build/test/disk_writer_test3");

	Cleanup cl;

	path const disk = "build/test/disk_writer_test3.disk";
	path const partition = "build/test/disk_writer_test3.partition";

	cl.add(disk);
	cl.add(partition);

	/* Using empty files here still triggers the bug and is much quicker than using random data */
	make_empty_file(disk,      31043616768LL);
	make_empty_file(partition, 31043571712LL);

	vector<boost::filesystem::path> const dcps = {
		TestPaths::private_data() / "xm",
		TestPaths::private_data() / "JourneyToJah_TLR-1_F_EN-DE-FR_CH_51_2K_LOK_20140225_DGL_SMPTE_OV"
	};
	dcpomatic::write(dcps, disk.string(), partition.string(), nullptr);

	BOOST_CHECK_EQUAL(system("/sbin/e2fsck -fn build/test/disk_writer_test3.partition"), 0);

	path const check = "build/test/disk_writer_test3";
	create_directory(check);
	cl.add(check);

	for (auto dcp: dcps) {
		for (auto original: directory_iterator(dcp)) {
			auto path_in_copy = dcp.filename() / original.path().filename();
			auto path_in_check = check / original.path().filename();
			system("e2cp " + partition.string() + ":" + path_in_copy.string() + " " + path_in_check.string());
			check_file(original.path(), path_in_check);
		}
	}

	cl.run();
}
