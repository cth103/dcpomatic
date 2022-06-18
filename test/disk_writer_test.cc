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
	make_random_file(disk, 256 * 1024 * 1024);
	make_random_file(partition, 256 * 1024 * 1024);

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

	make_random_file(disk, 4LL * 1024LL * 1024LL * 1024LL);
	make_random_file(partition, 4LL * 1024LL * 1024LL * 1024LL);

	auto const dcp = TestPaths::private_data() / "xm";
	dcpomatic::write(dcp, disk.string(), partition.string(), 0);

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


BOOST_AUTO_TEST_CASE (osx_drive_identification_test)
{
	vector<OSXDisk> disks;

	auto disk = [&disks](string mount_point, string media_path, bool whole, std::vector<boost::filesystem::path> mount_points)
	{
		auto mp = analyse_osx_media_path (media_path);
		if (mp) {
			disks.push_back({mount_point, {}, {}, *mp, whole, mount_points, 0});
		}
	};

	auto find_unmounted = [](vector<OSXDisk> disks) {
		auto drives = osx_disks_to_drives (disks);
		vector<Drive> unmounted;
		std::copy_if (drives.begin(), drives.end(), std::back_inserter(unmounted), [](Drive const& drive) { return !drive.mounted(); });
		return unmounted;
	};

	disk("/dev/disk4s1", "IODeviceTree:/arm-io@10F00000/apcie@90000000/pci-bridge1@1/pcie-xhci@0/@7:1", false, {});
	disk("/dev/disk4", "IODeviceTree:/arm-io@10F00000/apcie@90000000/pci-bridge1@1/pcie-xhci@0/@7:0", true, {});
	disk("/dev/disk0", "IODeviceTree:/arm-io@10F00000/ans@77400000/iop-ans-nub/AppleANS3NVMeController/@1:0", true, {});
	disk("/dev/disk0s1", "IODeviceTree:/arm-io@10F00000/ans@77400000/iop-ans-nub/AppleANS3NVMeController/@1:1", false, {});
	disk("/dev/disk0s2", "IODeviceTree:/arm-io@10F00000/ans@77400000/iop-ans-nub/AppleANS3NVMeController/@1:2", false, {});
	disk("/dev/disk0s3", "IODeviceTree:/arm-io@10F00000/ans@77400000/iop-ans-nub/AppleANS3NVMeController/@1:3", false, {});
	disk("/dev/disk1", "IOService:/AppleARMPE/arm-io@10F00000/AppleT810xIO/ans@77400000/AppleASCWrapV4/iop-ans-nub/RTBuddyV2/RTBuddyService/AppleANS3NVMeController/NS_01@1/IOBlockStorageDriver/APPLE SSD AP0512Q Media/IOGUIDPartitionScheme/iBootSystemContainer@1/AppleAPFSContainerScheme/AppleAPFSMedia", true, {});
	disk("/dev/disk2", "IOService:/AppleARMPE/arm-io@10F00000/AppleT810xIO/ans@77400000/AppleASCWrapV4/iop-ans-nub/RTBuddyV2/RTBuddyService/AppleANS3NVMeController/NS_01@1/IOBlockStorageDriver/APPLE SSD AP0512Q Media/IOGUIDPartitionScheme/RecoveryOSContainer@3/AppleAPFSContainerScheme/AppleAPFSMedia", true, {});
	disk("/dev/disk3", "IOService:/AppleARMPE/arm-io@10F00000/AppleT810xIO/ans@77400000/AppleASCWrapV4/iop-ans-nub/RTBuddyV2/RTBuddyService/AppleANS3NVMeController/NS_01@1/IOBlockStorageDriver/APPLE SSD AP0512Q Media/IOGUIDPartitionScheme/Container@2/AppleAPFSContainerScheme/AppleAPFSMedia", false, {});
	disk("/dev/disk1s1", "IOService:/AppleARMPE/arm-io@10F00000/AppleT810xIO/ans@77400000/AppleASCWrapV4/iop-ans-nub/RTBuddyV2/RTBuddyService/AppleANS3NVMeController/NS_01@1/IOBlockStorageDriver/APPLE SSD AP0512Q Media/IOGUIDPartitionScheme/iBootSystemContainer@1/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/iSCPreboot@1", false, {});
	disk("/dev/disk1s2", "IOService:/AppleARMPE/arm-io@10F00000/AppleT810xIO/ans@77400000/AppleASCWrapV4/iop-ans-nub/RTBuddyV2/RTBuddyService/AppleANS3NVMeController/NS_01@1/IOBlockStorageDriver/APPLE SSD AP0512Q Media/IOGUIDPartitionScheme/iBootSystemContainer@1/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/xART@2", false, {});
	disk("/dev/disk1s3", "IOService:/AppleARMPE/arm-io@10F00000/AppleT810xIO/ans@77400000/AppleASCWrapV4/iop-ans-nub/RTBuddyV2/RTBuddyService/AppleANS3NVMeController/NS_01@1/IOBlockStorageDriver/APPLE SSD AP0512Q Media/IOGUIDPartitionScheme/iBootSystemContainer@1/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/Hardware@3", false, {});
	disk("/dev/disk1s4", "IOService:/AppleARMPE/arm-io@10F00000/AppleT810xIO/ans@77400000/AppleASCWrapV4/iop-ans-nub/RTBuddyV2/RTBuddyService/AppleANS3NVMeController/NS_01@1/IOBlockStorageDriver/APPLE SSD AP0512Q Media/IOGUIDPartitionScheme/iBootSystemContainer@1/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/Recovery@4", false, {});
	disk("/dev/disk2s1", "IOService:/AppleARMPE/arm-io@10F00000/AppleT810xIO/ans@77400000/AppleASCWrapV4/iop-ans-nub/RTBuddyV2/RTBuddyService/AppleANS3NVMeController/NS_01@1/IOBlockStorageDriver/APPLE SSD AP0512Q Media/IOGUIDPartitionScheme/RecoveryOSContainer@3/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/Recovery@1", false, {});
	disk("/dev/disk2s2", "IOService:/AppleARMPE/arm-io@10F00000/AppleT810xIO/ans@77400000/AppleASCWrapV4/iop-ans-nub/RTBuddyV2/RTBuddyService/AppleANS3NVMeController/NS_01@1/IOBlockStorageDriver/APPLE SSD AP0512Q Media/IOGUIDPartitionScheme/RecoveryOSContainer@3/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/Update@2", false, {});
	disk("/dev/disk3s1", "IOService:/AppleARMPE/arm-io@10F00000/AppleT810xIO/ans@77400000/AppleASCWrapV4/iop-ans-nub/RTBuddyV2/RTBuddyService/AppleANS3NVMeController/NS_01@1/IOBlockStorageDriver/APPLE SSD AP0512Q Media/IOGUIDPartitionScheme/Container@2/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/Macintosh HD@1", false, {});
	disk("/dev/disk3s4", "IOService:/AppleARMPE/arm-io@10F00000/AppleT810xIO/ans@77400000/AppleASCWrapV4/iop-ans-nub/RTBuddyV2/RTBuddyService/AppleANS3NVMeController/NS_01@1/IOBlockStorageDriver/APPLE SSD AP0512Q Media/IOGUIDPartitionScheme/Container@2/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/Update@4", false, {"/System/Volumes/Update"});
	disk("/dev/disk3s5", "IOService:/AppleARMPE/arm-io@10F00000/AppleT810xIO/ans@77400000/AppleASCWrapV4/iop-ans-nub/RTBuddyV2/RTBuddyService/AppleANS3NVMeController/NS_01@1/IOBlockStorageDriver/APPLE SSD AP0512Q Media/IOGUIDPartitionScheme/Container@2/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/Data@5", false, {"/System/Volumes/Data"});
	disk("/dev/disk3s2", "IOService:/AppleARMPE/arm-io@10F00000/AppleT810xIO/ans@77400000/AppleASCWrapV4/iop-ans-nub/RTBuddyV2/RTBuddyService/AppleANS3NVMeController/NS_01@1/IOBlockStorageDriver/APPLE SSD AP0512Q Media/IOGUIDPartitionScheme/Container@2/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/Preboot@2", false, {"/System/Volumes/Preboot"});
	disk("/dev/disk3s3", "IOService:/AppleARMPE/arm-io@10F00000/AppleT810xIO/ans@77400000/AppleASCWrapV4/iop-ans-nub/RTBuddyV2/RTBuddyService/AppleANS3NVMeController/NS_01@1/IOBlockStorageDriver/APPLE SSD AP0512Q Media/IOGUIDPartitionScheme/Container@2/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/Recovery@3", false, {});
	disk("/dev/disk3s6", "IOService:/AppleARMPE/arm-io@10F00000/AppleT810xIO/ans@77400000/AppleASCWrapV4/iop-ans-nub/RTBuddyV2/RTBuddyService/AppleANS3NVMeController/NS_01@1/IOBlockStorageDriver/APPLE SSD AP0512Q Media/IOGUIDPartitionScheme/Container@2/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/VM@6", false, {"/System/Volumes/VM"});
	disk("/dev/disk3s1s1", "IOService:/AppleARMPE/arm-io@10F00000/AppleT810xIO/ans@77400000/AppleASCWrapV4/iop-ans-nub/RTBuddyV2/RTBuddyService/AppleANS3NVMeController/NS_01@1/IOBlockStorageDriver/APPLE SSD AP0512Q Media/IOGUIDPartitionScheme/Container@2/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/Macintosh HD@1/com.apple.os.update-EA882DCA7A28EBA0A6E94689836BB10D77D84D1AEE2468E17775A447AA815278@1", false, {"/"});

	vector<Drive> writeable = find_unmounted (disks);
	BOOST_REQUIRE_EQUAL (writeable.size(), 1U);
	BOOST_CHECK_EQUAL (writeable[0].device(), "/dev/disk4");

	disks.clear ();
	disk("/dev/disk4s1", "IOService:/IOResources/IOHDIXController/IOHDIXHDDriveOutKernel@0/IODiskImageBlockStorageDeviceOutKernel/IOBlockStorageDriver/Apple UDIF read-only compressed (zlib) Media/IOGUIDPartitionScheme/disk image@1", false, {});
	disk("/dev/disk4", "IOService:/IOResources/IOHDIXController/IOHDIXHDDriveOutKernel@0/IODiskImageBlockStorageDeviceOutKernel/IOBlockStorageDriver/Apple UDIF read-only compressed (zlib) Media", true, {});
	disk("/dev/disk3s1", "IODeviceTree:/PCI0@0/XHC1@14/@2:1", false, {});
	disk("/dev/disk3", "IODeviceTree:/PCI0@0/XHC1@14/@2:0", true, {});
	disk("/dev/disk0", "IODeviceTree:/PCI0@0/SATA@1F,2/PRT1@1/PMP@0/@0:0", true, {});
	disk("/dev/disk0s1", "IODeviceTree:/PCI0@0/SATA@1F,2/PRT1@1/PMP@0/@0:1", false, {});
	disk("/dev/disk0s2", "IODeviceTree:/PCI0@0/SATA@1F,2/PRT1@1/PMP@0/@0:2", false, {"/Volumes/Macintosh HD"});
	disk("/dev/disk0s3", "IODeviceTree:/PCI0@0/SATA@1F,2/PRT1@1/PMP@0/@0:3", false, {});
	disk("/dev/disk0s4", "IODeviceTree:/PCI0@0/SATA@1F,2/PRT1@1/PMP@0/@0:4", false, {});
	disk("/dev/disk0s5", "IODeviceTree:/PCI0@0/SATA@1F,2/PRT1@1/PMP@0/@0:5", false, {"/Volumes/High Sierra"});
	disk("/dev/disk0s6", "IODeviceTree:/PCI0@0/SATA@1F,2/PRT1@1/PMP@0/@0:6", false, {});
	disk("/dev/disk0s7", "IODeviceTree:/PCI0@0/SATA@1F,2/PRT1@1/PMP@0/@0:7", false, {"/Volumes/Recovery HD"});
	disk("/dev/disk1", "IOService:/AppleACPIPlatformExpert/PCI0@0/AppleACPIPCI/SATA@1F,2/AppleIntelPchSeriesAHCI/PRT1@1/IOAHCIDevice@0/AppleAHCIDiskDriver/IOAHCIBlockStorageDevice/IOBlockStorageDriver/APPLE HDD ST500LM012 Media/IOGUIDPartitionScheme/Untitled 3@3/AppleAPFSContainerScheme/AppleAPFSMedia", true, {});
	disk("/dev/disk", "IOService:/AppleACPIPlatformExpert/PCI0@0/AppleACPIPCI/SATA@1F,2/AppleIntelPchSeriesAHCI/PRT1@1/IOAHCIDevice@0/AppleAHCIDiskDriver/IOAHCIBlockStorageDevice/IOBlockStorageDriver/APPLE HDD ST500LM012 Media/IOGUIDPartitionScheme/Untitled 4@4/AppleAPFSContainerScheme/AppleAPFSMedia", true, {});
	disk("/dev/disk1s1", "IOService:/AppleACPIPlatformExpert/PCI0@0/AppleACPIPCI/SATA@1F,2/AppleIntelPchSeriesAHCI/PRT1@1/IOAHCIDevice@0/AppleAHCIDiskDriver/IOAHCIBlockStorageDevice/IOBlockStorageDriver/APPLE HDD ST500LM012 Media/IOGUIDPartitionScheme/Untitled 3@3/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/Untitled - Data@1", false, {"/Volumes/Untitled - Data"});
	disk("/dev/disk1s2", "IOService:/AppleACPIPlatformExpert/PCI0@0/AppleACPIPCI/SATA@1F,2/AppleIntelPchSeriesAHCI/PRT1@1/IOAHCIDevice@0/AppleAHCIDiskDriver/IOAHCIBlockStorageDevice/IOBlockStorageDriver/APPLE HDD ST500LM012 Media/IOGUIDPartitionScheme/Untitled 3@3/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/Preboot@2", false, {});
	disk("/dev/disk1s3", "IOService:/AppleACPIPlatformExpert/PCI0@0/AppleACPIPCI/SATA@1F,2/AppleIntelPchSeriesAHCI/PRT1@1/IOAHCIDevice@0/AppleAHCIDiskDriver/IOAHCIBlockStorageDevice/IOBlockStorageDriver/APPLE HDD ST500LM012 Media/IOGUIDPartitionScheme/Untitled 3@3/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/Recovery@3", false, {});
	disk("/dev/disk1s4", "IOService:/AppleACPIPlatformExpert/PCI0@0/AppleACPIPCI/SATA@1F,2/AppleIntelPchSeriesAHCI/PRT1@1/IOAHCIDevice@0/AppleAHCIDiskDriver/IOAHCIBlockStorageDevice/IOBlockStorageDriver/APPLE HDD ST500LM012 Media/IOGUIDPartitionScheme/Untitled 3@3/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/VM@4", false, {});
	disk("/dev/disk1s5", "IOService:/AppleACPIPlatformExpert/PCI0@0/AppleACPIPCI/SATA@1F,2/AppleIntelPchSeriesAHCI/PRT1@1/IOAHCIDevice@0/AppleAHCIDiskDriver/IOAHCIBlockStorageDevice/IOBlockStorageDriver/APPLE HDD ST500LM012 Media/IOGUIDPartitionScheme/Untitled 3@3/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/Untitled@5", false, {"/Volumes/Untitled"});
	disk("/dev/disk2s1", "IOService:/AppleACPIPlatformExpert/PCI0@0/AppleACPIPCI/SATA@1F,2/AppleIntelPchSeriesAHCI/PRT1@1/IOAHCIDevice@0/AppleAHCIDiskDriver/IOAHCIBlockStorageDevice/IOBlockStorageDriver/APPLE HDD ST500LM012 Media/IOGUIDPartitionScheme/Untitled 4@4/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/Catalina - Data@1", false, {});
	disk("/dev/disk2s2", "IOService:/AppleACPIPlatformExpert/PCI0@0/AppleACPIPCI/SATA@1F,2/AppleIntelPchSeriesAHCI/PRT1@1/IOAHCIDevice@0/AppleAHCIDiskDriver/IOAHCIBlockStorageDevice/IOBlockStorageDriver/APPLE HDD ST500LM012 Media/IOGUIDPartitionScheme/Untitled 4@4/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/Preboot@2", false, {});
	disk("/dev/disk2s3", "IOService:/AppleACPIPlatformExpert/PCI0@0/AppleACPIPCI/SATA@1F,2/AppleIntelPchSeriesAHCI/PRT1@1/IOAHCIDevice@0/AppleAHCIDiskDriver/IOAHCIBlockStorageDevice/IOBlockStorageDriver/APPLE HDD ST500LM012 Media/IOGUIDPartitionScheme/Untitled 4@4/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/Recovery@3", false, {});
	disk("/dev/disk2s4", "IOService:/AppleACPIPlatformExpert/PCI0@0/AppleACPIPCI/SATA@1F,2/AppleIntelPchSeriesAHCI/PRT1@1/IOAHCIDevice@0/AppleAHCIDiskDriver/IOAHCIBlockStorageDevice/IOBlockStorageDriver/APPLE HDD ST500LM012 Media/IOGUIDPartitionScheme/Untitled 4@4/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/VM@4", false, {"/private/var/vm"});
	disk("/dev/disk2s5", "IOService:/AppleACPIPlatformExpert/PCI0@0/AppleACPIPCI/SATA@1F,2/AppleIntelPchSeriesAHCI/PRT1@1/IOAHCIDevice@0/AppleAHCIDiskDriver/IOAHCIBlockStorageDevice/IOBlockStorageDriver/APPLE HDD ST500LM012 Media/IOGUIDPartitionScheme/Untitled 4@4/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/Catalina@5", false, {"/"});

	writeable = find_unmounted (disks);
	BOOST_REQUIRE_EQUAL (writeable.size(), 1U);
	BOOST_CHECK_EQUAL (writeable[0].device(), "/dev/disk3");

	disks.clear ();
	disk("/dev/disk7", "IOService:/IOResources/IOHDIXController/IOHDIXHDDriveOutKernel@3/IODiskImageBlockStorageDeviceOutKernel/IOBlockStorageDriver/Apple UDIF read-only compressed (zlib) Media", true, {});
	disk("/dev/disk7s1", "IOService:/IOResources/IOHDIXController/IOHDIXHDDriveOutKernel@3/IODiskImageBlockStorageDeviceOutKernel/IOBlockStorageDriver/Apple UDIF read-only compressed (zlib) Media/IOGUIDPartitionScheme/disk image@1", false, {});
	disk("/dev/disk6s1", "MediaPathKey is IOService:/IOResources/IOHDIXController/IOHDIXHDDriveOutKernel@2/IODiskImageBlockStorageDeviceOutKernel/IOBlockStorageDriver/Apple UDIF read-only compressed (zlib) Media/IOGUIDPartitionScheme/disk image@1", false, {});
	disk("/dev/disk6", "IOService:/IOResources/IOHDIXController/IOHDIXHDDriveOutKernel@2/IODiskImageBlockStorageDeviceOutKernel/IOBlockStorageDriver/Apple UDIF read-only compressed (zlib) Media", true, {});
	disk("/dev/disk5s1", "IOService:/IOResources/IOHDIXController/IOHDIXHDDriveOutKernel@1/IODiskImageBlockStorageDeviceOutKernel/IOBlockStorageDriver/Apple UDIF read-only compressed (zlib) Media/IOGUIDPartitionScheme/disk image@1", false, {});
	disk("/dev/disk5", "IOService:/IOResources/IOHDIXController/IOHDIXHDDriveOutKernel@1/IODiskImageBlockStorageDeviceOutKernel/IOBlockStorageDriver/Apple UDIF read-only compressed (zlib) Media", true, {});
	disk("/dev/disk4s1", "IOService:/IOResources/IOHDIXController/IOHDIXHDDriveOutKernel@0/IODiskImageBlockStorageDeviceOutKernel/IOBlockStorageDriver/Apple UDIF read-only compressed (zlib) Media/IOGUIDPartitionScheme/disk image@1", false, {});
	disk("/dev/disk4", "IOService:/IOResources/IOHDIXController/IOHDIXHDDriveOutKernel@0/IODiskImageBlockStorageDeviceOutKernel/IOBlockStorageDriver/Apple UDIF read-only compressed (zlib) Media", true, {});
	disk("/dev/disk0", "IODeviceTree:/PCI0@1e0000/pci8086,2829@1F,2/PRT3@3/PMP@0/@0:0", true, {});
	disk("/dev/disk2", "IODeviceTree:/PCI0@1e0000/pci8086,2829@1F,2/PRT1@1/PMP@0/@0:0", true, {});
	disk("/dev/disk1", "IODeviceTree:/PCI0@1e0000/pci8086,2829@1F,2/PRT0@0/PMP@0/@0:0", true, {});
	disk("/dev/disk1s1", "IODeviceTree:/PCI0@1e0000/pci8086,2829@1F,2/PRT0@0/PMP@0/@0:1", false, {"/Volumes/EFI"});
	disk("/dev/disk2s1", "IODeviceTree:/PCI0@1e0000/pci8086,2829@1F,2/PRT1@1/PMP@0/@0:1", false, {});
	disk("/dev/disk2s2", "IODeviceTree:/PCI0@1e0000/pci8086,2829@1F,2/PRT1@1/PMP@0/@0:2", false, {});
	disk("/dev/disk3", "IOService:/AppleACPIPlatformExpert/PCI0@1e0000/AppleACPIPCI/pci8086,2829@1F,2/AppleAHCI/PRT1@1/IOAHCIDevice@0/AppleAHCIDiskDriver/IOAHCIBlockStorageDevice/IOBlockStorageDriver/VBOX HARDDISK Media/IOGUIDPartitionScheme/disk image@2/AppleAPFSContainerScheme/AppleAPFSMedia", false, {});
	disk("/dev/disk3s1", "IOService:/AppleACPIPlatformExpert/PCI0@1e0000/AppleACPIPCI/pci8086,2829@1F,2/AppleAHCI/PRT1@1/IOAHCIDevice@0/AppleAHCIDiskDriver/IOAHCIBlockStorageDevice/IOBlockStorageDriver/VBOX HARDDISK Media/IOGUIDPartitionScheme/disk image@2/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/macOS - Data@1", false, {"/System/Volumes/Data"});
	disk("/dev/disk3s2", "IOService:/AppleACPIPlatformExpert/PCI0@1e0000/AppleACPIPCI/pci8086,2829@1F,2/AppleAHCI/PRT1@1/IOAHCIDevice@0/AppleAHCIDiskDriver/IOAHCIBlockStorageDevice/IOBlockStorageDriver/VBOX HARDDISK Media/IOGUIDPartitionScheme/disk image@2/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/Preboot@2", false, {"/System/Volumes/Preboot"});
	disk("/dev/disk3s3", "IOService:/AppleACPIPlatformExpert/PCI0@1e0000/AppleACPIPCI/pci8086,2829@1F,2/AppleAHCI/PRT1@1/IOAHCIDevice@0/AppleAHCIDiskDriver/IOAHCIBlockStorageDevice/IOBlockStorageDriver/VBOX HARDDISK Media/IOGUIDPartitionScheme/disk image@2/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/Recovery@3", false, {});
	disk("/dev/disk3s4", "IOService:/AppleACPIPlatformExpert/PCI0@1e0000/AppleACPIPCI/pci8086,2829@1F,2/AppleAHCI/PRT1@1/IOAHCIDevice@0/AppleAHCIDiskDriver/IOAHCIBlockStorageDevice/IOBlockStorageDriver/VBOX HARDDISK Media/IOGUIDPartitionScheme/disk image@2/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/VM@4", false, {"/System/Volumes/VM"});
	disk("/dev/disk3s5", "IOService:/AppleACPIPlatformExpert/PCI0@1e0000/AppleACPIPCI/pci8086,2829@1F,2/AppleAHCI/PRT1@1/IOAHCIDevice@0/AppleAHCIDiskDriver/IOAHCIBlockStorageDevice/IOBlockStorageDriver/VBOX HARDDISK Media/IOGUIDPartitionScheme/disk image@2/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/macOS@5", false, {});
	disk("/dev/disk3s6", "IOService:/AppleACPIPlatformExpert/PCI0@1e0000/AppleACPIPCI/pci8086,2829@1F,2/AppleAHCI/PRT1@1/IOAHCIDevice@0/AppleAHCIDiskDriver/IOAHCIBlockStorageDevice/IOBlockStorageDriver/VBOX HARDDISK Media/IOGUIDPartitionScheme/disk image@2/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/Update@6", false, {"/System/Volumes/Update"});
	disk("/dev/disk3s5s1", "IOService:/AppleACPIPlatformExpert/PCI0@1e0000/AppleACPIPCI/pci8086,2829@1F,2/AppleAHCI/PRT1@1/IOAHCIDevice@0/AppleAHCIDiskDriver/IOAHCIBlockStorageDevice/IOBlockStorageDriver/VBOX HARDDISK Media/IOGUIDPartitionScheme/disk image@2/AppleAPFSContainerScheme/AppleAPFSMedia/AppleAPFSContainer/macOS@5/com.apple.os.update-5523D8E63431315F9F949CCDD0274BF797F5CEE4EAF616D4C66A01B8D6A83C7B@1", false, {"/"});

	writeable = find_unmounted (disks);
	BOOST_REQUIRE_EQUAL (writeable.size(), 1U);
	BOOST_CHECK_EQUAL (writeable[0].device(), "/dev/disk0");
}

