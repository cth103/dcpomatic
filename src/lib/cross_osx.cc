/*
    Copyright (C) 2012-2020 Carl Hetherington <cth@carlh.net>

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
#include "warnings.h"
#include <dcp/raw_convert.h>
#include <glib.h>
extern "C" {
#include <libavformat/avio.h>
}
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
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
using std::wstring;
using std::make_pair;
using std::vector;
using std::cerr;
using std::cout;
using std::runtime_error;
using std::map;
using boost::shared_ptr;
using boost::optional;
using boost::function;

/** @param s Number of seconds to sleep for */
void
dcpomatic_sleep_seconds (int s)
{
	sleep (s);
}

void
dcpomatic_sleep_milliseconds (int ms)
{
	usleep (ms * 1000);
}

/** @return A string of CPU information (model name etc.) */
string
cpu_info ()
{
	string info;

	char buffer[64];
	size_t N = sizeof (buffer);
	if (sysctlbyname ("machdep.cpu.brand_string", buffer, &N, 0, 0) == 0) {
		info = buffer;
	}

	return info;
}


boost::filesystem::path
directory_containing_executable ()
{
#if BOOST_VERSION >= 106100
	return boost::dll::program_location().parent_path();
#else
	uint32_t size = 1024;
	char buffer[size];
	if (_NSGetExecutablePath (buffer, &size)) {
		throw runtime_error ("_NSGetExecutablePath failed");
	}

	boost::filesystem::path path (buffer);
	path = boost::filesystem::canonical (path);
	return path.parent_path ();
#endif
}


boost::filesystem::path
resources_path ()
{
	return directory_containing_executable().parent_path() / "Resources";
}


boost::filesystem::path
xsd_path ()
{
	return resources_path() / "xsd";
}


boost::filesystem::path
tags_path ()
{
	return resources_path() / "tags";
}


void
run_ffprobe (boost::filesystem::path content, boost::filesystem::path out)
{
	boost::filesystem::path path = directory_containing_executable () / "ffprobe";

	string ffprobe = "\"" + path.string() + "\" \"" + content.string() + "\" 2> \"" + out.string() + "\"";
	LOG_GENERAL (N_("Probing with %1"), ffprobe);
	system (ffprobe.c_str ());
}


list<pair<string, string> >
mount_info ()
{
	list<pair<string, string> > m;
	return m;
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

/* Apparently there is no way to create an ofstream using a UTF-8
   filename under Windows.  We are hence reduced to using fopen
   with this wrapper.
*/
FILE *
fopen_boost (boost::filesystem::path p, string t)
{
        return fopen (p.c_str(), t.c_str ());
}

int
dcpomatic_fseek (FILE* stream, int64_t offset, int whence)
{
	return fseek (stream, offset, whence);
}

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
	boost::filesystem::path batch = directory_containing_executable();
	batch = batch.parent_path (); // MacOS
	batch = batch.parent_path (); // Contents
	batch = batch.parent_path (); // DCP-o-matic.app
	batch = batch.parent_path (); // Applications
	batch /= app;
	batch /= "Contents";
	batch /= "MacOS";
	batch /= executable;

	pid_t pid = fork ();
	if (pid == 0) {
		int const r = system (batch.string().c_str());
		exit (WEXITSTATUS (r));
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


uint64_t
thread_id ()
{
	return (uint64_t) pthread_self ();
}

int
avio_open_boost (AVIOContext** s, boost::filesystem::path file, int flags)
{
	return avio_open (s, file.c_str(), flags);
}

boost::filesystem::path
home_directory ()
{
		return getenv("HOME");
}

string
command_and_read (string)
{
	return "";
}

/** @return true if this process is a 32-bit one running on a 64-bit-capable OS */
bool
running_32_on_64 ()
{
	/* I'm assuming nobody does this on OS X */
	return false;
}

static optional<string>
get_vendor (CFDictionaryRef& description)
{
	void const* str = CFDictionaryGetValue (description, kDADiskDescriptionDeviceVendorKey);
	if (!str) {
		return optional<string>();
	}

	string s = CFStringGetCStringPtr ((CFStringRef) str, kCFStringEncodingUTF8);
	boost::algorithm::trim (s);
	return s;
}

static optional<string>
get_model (CFDictionaryRef& description)
{
	void const* str = CFDictionaryGetValue (description, kDADiskDescriptionDeviceModelKey);
	if (!str) {
		return optional<string>();
	}

	string s = CFStringGetCStringPtr ((CFStringRef) str, kCFStringEncodingUTF8);
	boost::algorithm::trim (s);
	return s;
}

struct MediaPath
{
	bool real;       ///< true for a "real" disk, false for a synthesized APFS one
	std::string prt; ///< "PRT" entry from the media path
};

static optional<MediaPath>
analyse_media_path (CFDictionaryRef& description)
{
	using namespace boost::algorithm;

	void const* str = CFDictionaryGetValue (description, kDADiskDescriptionMediaPathKey);
	if (!str) {
		LOG_DISK_NC("There is no MediaPathKey");
		return optional<MediaPath>();
	}

	string path(CFStringGetCStringPtr((CFStringRef) str, kCFStringEncodingUTF8));
	LOG_DISK("MediaPathKey is %1", path);

	if (path.find("/IOHDIXController") != string::npos) {
		/* This is a disk image, so we completely ignore it */
		LOG_DISK_NC("Ignoring this as it seems to be a disk image");
		return optional<MediaPath>();
	}

	MediaPath mp;
	if (starts_with(path, "IODeviceTree:")) {
		mp.real = true;
	} else if (starts_with(path, "IOService:")) {
		mp.real = false;
	} else {
		return optional<MediaPath>();
	}

	vector<string> bits;
	split(bits, path, boost::is_any_of("/"));
	BOOST_FOREACH (string i, bits) {
		if (starts_with(i, "PRT")) {
			mp.prt = i;
		}
	}

	return mp;
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
	CFURLRef volume_path_key = (CFURLRef) CFDictionaryGetValue (description, kDADiskDescriptionVolumePathKey);
	char mount_path_buffer[1024];
	if (!CFURLGetFileSystemRepresentation(volume_path_key, false, (UInt8 *) mount_path_buffer, sizeof(mount_path_buffer))) {
		return boost::optional<boost::filesystem::path>();
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
 * this I am picking out what looks like a suitable identifier prefixed with PRT from the MediaContentKey.
 * If disk2s2 and disk3 have the same PRT code I am assuming they are linked.
 *
 * Lots of this is guesswork and may be broken.  In my defence the documentation that I have been able to
 * unearth is, to put it impolitely, crap.
 */

struct Disk
{
	string mount_point;
	optional<string> vendor;
	optional<string> model;
	bool real;
	string prt;
	bool whole;
	vector<boost::filesystem::path> mount_points;
	unsigned long size;
};

static void
disk_appeared (DADiskRef disk, void* context)
{
	const char* bsd_name = DADiskGetBSDName (disk);
	if (!bsd_name) {
		return;
	}
	LOG_DISK("%1 appeared", bsd_name);

	Disk this_disk;

	this_disk.mount_point = string("/dev/") + bsd_name;

	CFDictionaryRef description = DADiskCopyDescription (disk);

	this_disk.vendor = get_vendor (description);
	this_disk.model = get_model (description);
	LOG_DISK("Vendor/model: %1 %2", this_disk.vendor.get_value_or("[none]"), this_disk.model.get_value_or("[none]"));

	optional<MediaPath> media_path = analyse_media_path (description);
	if (!media_path) {
		LOG_DISK("Finding media path for %1 failed", bsd_name);
		return;
	}

	this_disk.real = media_path->real;
	this_disk.prt = media_path->prt;
	this_disk.whole = is_whole_drive (disk);
	optional<boost::filesystem::path> mp = mount_point (description);
	if (mp) {
		this_disk.mount_points.push_back (*mp);
	}

	LOG_DISK(
		"%1 prt %2 whole %3 mounted %4",
		 this_disk.real ? "Real" : "Synth",
		 this_disk.prt,
		 this_disk.whole ? "whole" : "part",
		 mp ? ("mounted at " + mp->string()) : "unmounted"
		);

	CFNumberGetValue ((CFNumberRef) CFDictionaryGetValue (description, kDADiskDescriptionMediaSizeKey), kCFNumberLongType, &this_disk.size);
	CFRelease (description);

	reinterpret_cast<vector<Disk>*>(context)->push_back(this_disk);
}

vector<Drive>
Drive::get ()
{
	using namespace boost::algorithm;
	vector<Disk> disks;

	DASessionRef session = DASessionCreate(kCFAllocatorDefault);
	if (!session) {
		return vector<Drive>();
	}

	DARegisterDiskAppearedCallback (session, NULL, disk_appeared, &disks);
	CFRunLoopRef run_loop = CFRunLoopGetCurrent ();
	DASessionScheduleWithRunLoop (session, run_loop, kCFRunLoopDefaultMode);
	CFRunLoopStop (run_loop);
	CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, 0);
	DAUnregisterCallback(session, (void *) disk_appeared, &disks);
	CFRelease(session);

	/* Mark disks containing mounted partitions as themselves mounted */
	BOOST_FOREACH (Disk& i, disks) {
		if (!i.whole) {
			continue;
		}
		BOOST_FOREACH (Disk& j, disks) {
			if (!j.mount_points.empty() && starts_with(j.mount_point, i.mount_point)) {
				LOG_DISK("Marking %1 as mounted because %2 is", i.mount_point, j.mount_point);
				std::copy(j.mount_points.begin(), j.mount_points.end(), back_inserter(i.mount_points));
			}
		}
	}

	/* Make a map of the PRT codes and mount points of mounted, synthesized disks */
	map<string, vector<boost::filesystem::path> > mounted_synths;
	BOOST_FOREACH (Disk& i, disks) {
		if (!i.real && !i.mount_points.empty()) {
			LOG_DISK("Found a mounted synth %1 with %2", i.mount_point, i.prt);
			mounted_synths[i.prt] = i.mount_points;
		}
	}

	/* Mark containers of those mounted synths as themselves mounted */
	BOOST_FOREACH (Disk& i, disks) {
		if (i.real) {
			map<string, vector<boost::filesystem::path> >::const_iterator j = mounted_synths.find(i.prt);
			if (j != mounted_synths.end()) {
				LOG_DISK("Marking %1 (%2) as mounted because it contains a mounted synth", i.mount_point, i.prt);
				std::copy(j->second.begin(), j->second.end(), back_inserter(i.mount_points));
			}
		}
	}

	vector<Drive> drives;
	BOOST_FOREACH (Disk& i, disks) {
		if (i.whole) {
			/* A whole disk that is not a container for a mounted synth */
			drives.push_back(Drive(i.mount_point, i.mount_points, i.size, i.vendor, i.model));
			LOG_DISK_NC(drives.back().log_summary());
		}
	}
	return drives;
}


boost::filesystem::path
config_path ()
{
	boost::filesystem::path p;
	p /= g_get_home_dir ();
	p /= "Library";
	p /= "Preferences";
	p /= "com.dcpomatic";
	p /= "2";
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

	DASessionRef session = DASessionCreate(kCFAllocatorDefault);
	if (!session) {
		return false;
	}

	DADiskRef disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session, _device.c_str());
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
DCPOMATIC_DISABLE_WARNINGS
	GetCurrentProcess (&serial);
DCPOMATIC_ENABLE_WARNINGS
	TransformProcessType (&serial, kProcessTransformToForegroundApplication);
}

