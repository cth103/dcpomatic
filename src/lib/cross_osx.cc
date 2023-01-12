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


#include "cross.h"
#include "compose.hpp"
#include "log.h"
#include "dcpomatic_log.h"
#include "config.h"
#include "exceptions.h"
#include <dcp/raw_convert.h>
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
	return boost::filesystem::canonical(boost::dll::program_location()).parent_path();
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
run_ffprobe (boost::filesystem::path content, boost::filesystem::path out)
{
	auto path = directory_containing_executable () / "ffprobe";

	string ffprobe = "\"" + path.string() + "\" \"" + content.string() + "\" 2> \"" + out.string() + "\"";
	LOG_GENERAL (N_("Probing with %1"), ffprobe);
	system (ffprobe.c_str ());
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
	start_tool ("dcpomatic2_batch", "DCP-o-matic\\ 2\\ Batch\\ Converter.app");
}


void
start_player ()
{
	start_tool ("dcpomatic2_player", "DCP-o-matic\\ 2\\ Player.app");
}


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


static optional<OSXMediaPath>
analyse_media_path (CFDictionaryRef& description)
{
	using namespace boost::algorithm;

	void const* str = CFDictionaryGetValue (description, kDADiskDescriptionMediaPathKey);
	if (!str) {
		LOG_DISK_NC("There is no MediaPathKey (no dictionary value)");
		return {};
	}

	auto path_key_cstr = CFStringGetCStringPtr((CFStringRef) str, kCFStringEncodingUTF8);
	if (!path_key_cstr) {
		LOG_DISK_NC("There is no MediaPathKey (no cstring)");
		return {};
	}

	string path(path_key_cstr);
	LOG_DISK("MediaPathKey is %1", path);
	return analyse_osx_media_path (path);
}


static bool
is_whole_drive (DADiskRef& disk)
{
	io_service_t service = DADiskCopyIOMedia (disk);
        CFTypeRef whole_media_ref = IORegistryEntryCreateCFProperty (service, CFSTR(kIOMediaWholeKey), kCFAllocatorDefault, 0);
	bool whole_media = false;
        if (whole_media_ref) {
		whole_media = CFBooleanGetValue((CFBooleanRef) whole_media_ref);
                CFRelease (whole_media_ref);
        }
	IOObjectRelease (service);
	return whole_media;
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


/* Here follows some rather intricate and (probably) fragile code to find the list of available
 * "real" drives on macOS that we might want to write a DCP to.
 *
 * We use the Disk Arbitration framework to give us a series of mount_points (/dev/disk0, /dev/disk1,
 * /dev/disk1s1 and so on) and we use the API to gather useful information about these mount_points into
 * a vector of Disk structs.
 *
 * Then we read the Disks that we found and try to derive a list of drives that we should offer to the
 * user, with details of whether those drives are currently mounted or not.
 *
 * At the basic level we find the "disk"-level mount_points, looking at whether any of their partitions are mounted.
 *
 * This is complicated enormously by recent-ish macOS versions' habit of making `synthesized' volumes which
 * reflect data in `real' partitions.  So, for example, we might have a real (physical) drive /dev/disk2 with
 * a partition /dev/disk2s2 whose content is made into a synthesized /dev/disk3, itself containing some partitions
 * which are mounted.  /dev/disk2s2 is not considered to be mounted, in this case.  So we need to know that
 * disk2s2 is related to disk3 so we can consider disk2s2 as mounted if any parts of disk3 are.  In order to do
 * this I am taking the first two parts of the IODeviceTree and seeing if they exist anywhere in a
 * IOService identifier.  If they do, I am assuming the IOService device is on the matching IODeviceTree device.
 *
 * Lots of this is guesswork and may be broken.  In my defence the documentation that I have been able to
 * unearth is, to put it impolitely, crap.
 */

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

	this_disk.device = string("/dev/") + bsd_name;
	LOG_DISK("Device is %1", this_disk.device);

	CFDictionaryRef description = DADiskCopyDescription (disk);

	this_disk.vendor = get_vendor (description);
	this_disk.model = get_model (description);
	LOG_DISK("Vendor/model: %1 %2", this_disk.vendor.get_value_or("[none]"), this_disk.model.get_value_or("[none]"));

	auto media_path = analyse_media_path (description);
	if (!media_path) {
		LOG_DISK("Finding media path for %1 failed", bsd_name);
		return;
	}

	this_disk.media_path = *media_path;
	this_disk.whole = is_whole_drive (disk);
	auto mp = mount_point (description);
	if (mp) {
		this_disk.mount_points.push_back (*mp);
	}

	LOG_DISK(
		"%1 %2 mounted at %3",
		 this_disk.media_path.real ? "Real" : "Synth",
		 this_disk.whole ? "whole" : "part",
		 mp ? mp->string() : "[nowhere]"
		);

	auto media_size_cstr = CFDictionaryGetValue (description, kDADiskDescriptionMediaSizeKey);
	if (!media_size_cstr) {
		LOG_DISK_NC("Could not read media size");
		return;
	}

	CFNumberGetValue ((CFNumberRef) media_size_cstr, kCFNumberLongType, &this_disk.size);
	CFRelease (description);

	reinterpret_cast<vector<OSXDisk>*>(context)->push_back(this_disk);
}


vector<Drive>
Drive::get ()
{
	using namespace boost::algorithm;
	vector<OSXDisk> disks;

	auto session = DASessionCreate(kCFAllocatorDefault);
	if (!session) {
		return {};
	}

	DARegisterDiskAppearedCallback (session, NULL, disk_appeared, &disks);
	auto run_loop = CFRunLoopGetCurrent ();
	DASessionScheduleWithRunLoop (session, run_loop, kCFRunLoopDefaultMode);
	CFRunLoopStop (run_loop);
	CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, 0);
	DAUnregisterCallback(session, (void *) disk_appeared, &disks);
	CFRelease(session);

	return osx_disks_to_drives (disks);
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


void done_callback(DADiskRef, DADissenterRef dissenter, void* context)
{
	LOG_DISK_NC("Unmount finished");
	bool* success = reinterpret_cast<bool*> (context);
	if (dissenter) {
		LOG_DISK("Error: %1", DADissenterGetStatus(dissenter));
		*success = false;
	} else {
		LOG_DISK_NC("Successful");
		*success = true;
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
	bool success = false;
	DADiskUnmount(disk, kDADiskUnmountOptionWhole, &done_callback, &success);
	CFRelease (disk);

	CFRunLoopRef run_loop = CFRunLoopGetCurrent ();
	DASessionScheduleWithRunLoop (session, run_loop, kCFRunLoopDefaultMode);
	CFRunLoopStop (run_loop);
	CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.5, 0);
	CFRunLoopRunInMode(kCFRunLoopDefaultMode, 5, 0);
	CFRelease(session);

	LOG_DISK_NC("End of unmount");
	return success;
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

