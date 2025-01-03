/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include "compose.hpp"
#include "config.h"
#include "cross.h"
#include "dcpomatic_log.h"
#include "exceptions.h"
#include "log.h"
#include "variant.h"
#include <dcp/filesystem.h>
#include <glib.h>
#include <boost/algorithm/string.hpp>
#include <boost/regex.hpp>
#if BOOST_VERSION >= 106100
#include <boost/dll/runtime_symbol_info.hpp>
#endif
#include <ApplicationServices/ApplicationServices.h>
#include <sys/sysctl.h>
#include <mach-o/dyld.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/storage/IOMedia.h>
#include <DiskArbitration/DADisk.h>
#include <DiskArbitration/DiskArbitration.h>
#include <CoreFoundation/CFURL.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fstream>
#include <cstring>

#include "i18n.h"


using std::pair;
using std::list;
using std::ifstream;
using std::string;
using std::make_pair;
using std::vector;
using std::cerr;
using std::cout;
using std::runtime_error;
using std::map;
using std::shared_ptr;
using boost::optional;
using std::function;


/** @return A string of CPU information (model name etc.) */
string
cpu_info ()
{
	string info;

	char buffer[64];
	size_t N = sizeof (buffer);
	if (sysctlbyname("machdep.cpu.brand_string", buffer, &N, 0, 0) == 0) {
		info = buffer;
	}

	return info;
}


boost::filesystem::path
directory_containing_executable ()
{
	return dcp::filesystem::canonical(boost::dll::program_location()).parent_path();
}


boost::filesystem::path
resources_path ()
{
	return directory_containing_executable().parent_path() / "Resources";
}


boost::filesystem::path
libdcp_resources_path ()
{
	return resources_path();
}


void
run_ffprobe(boost::filesystem::path content, boost::filesystem::path out, bool err, string args)
{
	auto path = directory_containing_executable () / "ffprobe";
	if (!dcp::filesystem::exists(path)) {
		/* This is a hack but we need ffprobe during tests */
		path = "/Users/ci/workspace/bin/ffprobe";
	}
	string const redirect = err ? "2>" : ">";

	auto const ffprobe = String::compose("\"%1\" %2 \"%3\" %4 \"%5\"", path, args.empty() ? " " : args, content.string(), redirect, out.string());
	LOG_GENERAL (N_("Probing with %1"), ffprobe);
	system (ffprobe.c_str());
}



list<pair<string, string>>
mount_info ()
{
	return {};
}


boost::filesystem::path
openssl_path ()
{
	return directory_containing_executable() / "openssl";
}


#ifdef DCPOMATIC_DISK
/* Note: this isn't actually used at the moment as the disk writer is started as a service */
boost::filesystem::path
disk_writer_path ()
{
	return directory_containing_executable() / "dcpomatic2_disk_writer";
}
#endif


void
Waker::nudge ()
{

}


Waker::Waker ()
{
	boost::mutex::scoped_lock lm (_mutex);
	IOPMAssertionCreateWithName (kIOPMAssertionTypeNoIdleSleep, kIOPMAssertionLevelOn, CFSTR ("Encoding DCP"), &_assertion_id);
}


Waker::~Waker ()
{
	boost::mutex::scoped_lock lm (_mutex);
	IOPMAssertionRelease (_assertion_id);
}


void
start_tool (string executable, string app)
{
	boost::algorithm::replace_all(app, " ", "\\ ");

	auto exe_path = directory_containing_executable();
	exe_path = exe_path.parent_path(); // Contents
	exe_path = exe_path.parent_path(); // DCP-o-matic 2.app
	exe_path = exe_path.parent_path(); // Applications
	exe_path /= app;
	exe_path /= "Contents";
	exe_path /= "MacOS";
	exe_path /= executable;

	pid_t pid = fork ();
	if (pid == 0) {
		LOG_GENERAL ("start_tool %1 %2 with path %3", executable, app, exe_path.string());
		int const r = system (exe_path.string().c_str());
		exit (WEXITSTATUS (r));
	} else if (pid == -1) {
		LOG_ERROR_NC("Fork failed in start_tool");
	}
}


void
start_batch_converter ()
{
	start_tool("dcpomatic2_batch", variant::dcpomatic_batch_converter_app());
}


void
start_player ()
{
	start_tool("dcpomatic2_player", variant::dcpomatic_player_app());
}


struct OSXDisk
{
	std::string bsd_name;
	std::string device;
	boost::optional<std::string> vendor;
	boost::optional<std::string> model;
	bool mounted;
	unsigned long size;
	bool system;
	bool writeable;
	bool partition;
};


static optional<string>
get_vendor (CFDictionaryRef& description)
{
	void const* str = CFDictionaryGetValue (description, kDADiskDescriptionDeviceVendorKey);
	if (!str) {
		return {};
	}

	auto c_str = CFStringGetCStringPtr ((CFStringRef) str, kCFStringEncodingUTF8);
	if (!c_str) {
		return {};
	}

	string s (c_str);
	boost::algorithm::trim (s);
	return s;
}


static optional<string>
get_model (CFDictionaryRef& description)
{
	void const* str = CFDictionaryGetValue (description, kDADiskDescriptionDeviceModelKey);
	if (!str) {
		return {};
	}

	auto c_str = CFStringGetCStringPtr ((CFStringRef) str, kCFStringEncodingUTF8);
	if (!c_str) {
		return {};
	}

	string s (c_str);
	boost::algorithm::trim (s);
	return s;
}


static optional<boost::filesystem::path>
mount_point (CFDictionaryRef& description)
{
	auto volume_path_key = (CFURLRef) CFDictionaryGetValue (description, kDADiskDescriptionVolumePathKey);
	if (!volume_path_key) {
		return {};
	}

	char mount_path_buffer[1024];
	if (!CFURLGetFileSystemRepresentation(volume_path_key, false, (UInt8 *) mount_path_buffer, sizeof(mount_path_buffer))) {
		return {};
	}
	return boost::filesystem::path(mount_path_buffer);
}


static bool
get_bool(CFDictionaryRef& description, void const* key)
{
	auto value = CFDictionaryGetValue(description, key);
	if (!value) {
		return false;
	}

	return CFBooleanGetValue(reinterpret_cast<CFBooleanRef>(value));
}


static void
disk_appeared (DADiskRef disk, void* context)
{
	auto bsd_name = DADiskGetBSDName (disk);
	if (!bsd_name) {
		LOG_DISK_NC("Disk with no BSDName appeared");
		return;
	}
	LOG_DISK("%1 appeared", bsd_name);

	OSXDisk this_disk;
	this_disk.bsd_name = bsd_name;

	this_disk.device = string("/dev/") + this_disk.bsd_name;
	LOG_DISK("Device is %1", this_disk.device);

	CFDictionaryRef description = DADiskCopyDescription (disk);

	this_disk.vendor = get_vendor (description);
	this_disk.model = get_model (description);
	LOG_DISK("Vendor/model: %1 %2", this_disk.vendor.get_value_or("[none]"), this_disk.model.get_value_or("[none]"));

	this_disk.mounted = static_cast<bool>(mount_point(description));

	auto media_size_cstr = CFDictionaryGetValue (description, kDADiskDescriptionMediaSizeKey);
	if (!media_size_cstr) {
		LOG_DISK_NC("Could not read media size");
		return;
	}

	this_disk.system = get_bool(description, kDADiskDescriptionDeviceInternalKey) && !get_bool(description, kDADiskDescriptionMediaRemovableKey);
	this_disk.writeable = get_bool(description, kDADiskDescriptionMediaWritableKey);
	this_disk.partition = string(this_disk.bsd_name).find("s", 5) != std::string::npos;

	LOG_DISK(
		"%1 %2 %3 %4 %5",
		this_disk.bsd_name,
		this_disk.system ? "system" : "non-system",
		this_disk.writeable ? "writeable" : "read-only",
		this_disk.partition ? "partition" : "drive",
		this_disk.mounted ? "mounted" : "not mounted"
		);

	CFNumberGetValue ((CFNumberRef) media_size_cstr, kCFNumberLongType, &this_disk.size);
	CFRelease (description);

	reinterpret_cast<vector<OSXDisk>*>(context)->push_back(this_disk);
}


vector<Drive>
Drive::get ()
{
	using namespace boost::algorithm;
	vector<OSXDisk> disks;

	LOG_DISK_NC("Drive::get() starts");

	auto session = DASessionCreate(kCFAllocatorDefault);
	if (!session) {
		return {};
	}

	LOG_DISK_NC("Drive::get() has session");

	DARegisterDiskAppearedCallback (session, NULL, disk_appeared, &disks);
	auto run_loop = CFRunLoopGetCurrent ();
	DASessionScheduleWithRunLoop (session, run_loop, kCFRunLoopDefaultMode);
	CFRunLoopStop (run_loop);
	CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, 0);
	DAUnregisterCallback(session, (void *) disk_appeared, &disks);
	CFRelease(session);

	/* Find all the drives (not partitions) - these OSXDisks can be either */
	vector<Drive> drives;
	for (auto const& disk: disks) {
		if (!disk.system && !disk.partition && disk.writeable) {
			LOG_DISK("Have a non-system writeable drive: %1", disk.device);
			drives.push_back({disk.device, disk.mounted, disk.size, disk.vendor, disk.model});
		}
	}

	/* Find mounted partitions and mark their drives mounted */
	for (auto const& disk: disks) {
		if (!disk.system && disk.partition && disk.mounted) {
			LOG_DISK("Have a mounted non-system partition: %1 (%2)", disk.device, disk.bsd_name);
			if (boost::algorithm::starts_with(disk.bsd_name, "disk")) {
				auto const second_s = disk.bsd_name.find('s', 4);
				if (second_s != std::string::npos) {
					/* We have a bsd_name of the form disk...s */
					auto const drive_device = "/dev/" + disk.bsd_name.substr(0, second_s);
					LOG_DISK("This belongs to the drive %1", drive_device);
					auto iter = std::find_if(drives.begin(), drives.end(), [drive_device](Drive const& drive) {
						return drive.device() == drive_device;
					});
					if (iter != drives.end()) {
						LOG_DISK("Marking %1 as mounted", drive_device);
						iter->set_mounted();
					}
				}
			}
		}
	}

	LOG_DISK("Drive::get() found %1 drives:", drives.size());
	for (auto const& drive: drives) {
		LOG_DISK("%1 %2 mounted=%3", drive.description(), drive.device(), drive.mounted() ? "yes" : "no");
	}

	return drives;
}


boost::filesystem::path
config_path (optional<string> version)
{
	boost::filesystem::path p;
	p /= g_get_home_dir ();
	p /= "Library";
	p /= "Preferences";
	p /= "com.dcpomatic";
	p /= "2";
	if (version) {
		p /= *version;
	}
	return p;
}


struct UnmountState
{
	bool success = false;
	bool callback = false;
};


void done_callback(DADiskRef, DADissenterRef dissenter, void* context)
{
	LOG_DISK_NC("Unmount finished");
	auto state = reinterpret_cast<UnmountState*>(context);
	state->callback = true;
	if (dissenter) {
		LOG_DISK("Error: %1", DADissenterGetStatus(dissenter));
	} else {
		LOG_DISK_NC("Successful");
		state->success = true;
	}
}


bool
Drive::unmount ()
{
	LOG_DISK_NC("Unmount operation started");

	auto session = DASessionCreate(kCFAllocatorDefault);
	if (!session) {
		return false;
	}

	auto disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session, _device.c_str());
	if (!disk) {
		return false;
	}
	LOG_DISK("Requesting unmount of %1 from %2", _device, thread_id());
	UnmountState state;
	DADiskUnmount(disk, kDADiskUnmountOptionWhole, &done_callback, &state);
	CFRelease (disk);

	CFRunLoopRef run_loop = CFRunLoopGetCurrent ();
	DASessionScheduleWithRunLoop (session, run_loop, kCFRunLoopDefaultMode);
	CFRunLoopStop (run_loop);
	CFRunLoopRunInMode(kCFRunLoopDefaultMode, 5, 0);
	CFRelease(session);

	if (!state.callback) {
		LOG_DISK_NC("End of unmount: timeout");
	} else {
		LOG_DISK("End of unmount: %1", state.success ? "success" : "failure");
	}
	return state.success;
}


void
disk_write_finished ()
{

}


void
make_foreground_application ()
{
	ProcessSerialNumber serial;
LIBDCP_DISABLE_WARNINGS
	GetCurrentProcess (&serial);
LIBDCP_ENABLE_WARNINGS
	TransformProcessType (&serial, kProcessTransformToForegroundApplication);
}


bool
show_in_file_manager (boost::filesystem::path, boost::filesystem::path select)
{
	int r = system (String::compose("open -R \"%1\"", select.string()).c_str());
	return static_cast<bool>(WEXITSTATUS(r));
}

