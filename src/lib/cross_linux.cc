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
#include "dcpomatic_log.h"
#include <dcp/raw_convert.h>
#include <glib.h>
extern "C" {
#include <libavformat/avio.h>
}
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/function.hpp>
#ifdef DCPOMATIC_DISK
#include <boost/dll/runtime_symbol_info.hpp>
#endif
#include <unistd.h>
#include <mntent.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fstream>

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

	/* This use of ifstream is ok; the filename can never
	   be non-Latin
	*/
	ifstream f ("/proc/cpuinfo");
	while (f.good ()) {
		string l;
		getline (f, l);
		if (boost::algorithm::starts_with (l, "model name")) {
			string::size_type const c = l.find (':');
			if (c != string::npos) {
				info = l.substr (c + 2);
			}
		}
	}

	return info;
}

boost::filesystem::path
shared_path ()
{
	char const * p = getenv ("DCPOMATIC_LINUX_SHARE_PREFIX");
	if (p) {
		return p;
	}
	return boost::filesystem::canonical (LINUX_SHARE_PREFIX);
}

void
run_ffprobe (boost::filesystem::path content, boost::filesystem::path out)
{
	string ffprobe = "ffprobe \"" + content.string() + "\" 2> \"" + out.string() + "\"";
	LOG_GENERAL (N_("Probing with %1"), ffprobe);
        system (ffprobe.c_str ());
}

list<pair<string, string> >
mount_info ()
{
	list<pair<string, string> > m;

	FILE* f = setmntent ("/etc/mtab", "r");
	if (!f) {
		return m;
	}

	while (true) {
		struct mntent* mnt = getmntent (f);
		if (!mnt) {
			break;
		}

		m.push_back (make_pair (mnt->mnt_dir, mnt->mnt_type));
	}

	endmntent (f);

	return m;
}

boost::filesystem::path
openssl_path ()
{
	return "dcpomatic2_openssl";
}

#ifdef DCPOMATIC_DISK
boost::filesystem::path
disk_writer_path ()
{
	return boost::dll::program_location().parent_path() / "dcpomatic2_disk_writer";
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

}

Waker::~Waker ()
{

}

void
start_tool (boost::filesystem::path dcpomatic, string executable, string)
{
	boost::filesystem::path batch = dcpomatic.parent_path() / executable;

	pid_t pid = fork ();
	if (pid == 0) {
		int const r = system (batch.string().c_str());
		exit (WEXITSTATUS (r));
	}
}

void
start_batch_converter (boost::filesystem::path dcpomatic)
{
	start_tool (dcpomatic, "dcpomatic2_batch", "DCP-o-matic\\ 2\\ Batch\\ Converter.app");
}

void
start_player (boost::filesystem::path dcpomatic)
{
	start_tool (dcpomatic, "dcpomatic2_player", "DCP-o-matic\\ 2\\ Player.app");
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
command_and_read (string cmd)
{
	FILE* pipe = popen (cmd.c_str(), "r");
	if (!pipe) {
		throw runtime_error ("popen failed");
	}

	string result;
	char buffer[128];
	try {
		while (fgets(buffer, sizeof(buffer), pipe)) {
			result += buffer;
		}
	} catch (...) {
		pclose (pipe);
		throw;
	}

	pclose (pipe);
	return result;
}

/** @return true if this process is a 32-bit one running on a 64-bit-capable OS */
bool
running_32_on_64 ()
{
	/* I'm assuming nobody does this on Linux */
	return false;
}


static
vector<pair<string, string> >
get_mounts (string prefix)
{
	vector<pair<string, string> > mounts;

	std::ifstream f("/proc/mounts");
	string line;
	while (f.good()) {
		getline(f, line);
		vector<string> bits;
		boost::algorithm::split (bits, line, boost::is_any_of(" "));
		if (bits.size() > 1 && boost::algorithm::starts_with(bits[0], prefix)) {
			boost::algorithm::replace_all (bits[1], "\\040", " ");
			mounts.push_back(make_pair(bits[0], bits[1]));
			LOG_DISK("Found mounted device %1 from prefix %2", bits[0], prefix);
		}
	}

	return mounts;
}


vector<Drive>
Drive::get ()
{
	vector<Drive> drives;

	using namespace boost::filesystem;
	vector<pair<string, string> > mounted_devices = get_mounts("/dev/");

	for (directory_iterator i = directory_iterator("/sys/block"); i != directory_iterator(); ++i) {
		string const name = i->path().filename().string();
		path device_type_file("/sys/block/" + name + "/device/type");
		optional<string> device_type;
		if (exists(device_type_file)) {
			device_type = dcp::file_to_string (device_type_file);
			boost::trim(*device_type);
		}
		/* Device type 5 is "SCSI_TYPE_ROM" in blkdev.h; seems usually to be a CD/DVD drive */
		if (!boost::algorithm::starts_with(name, "loop") && (!device_type || *device_type != "5")) {
			uint64_t const size = dcp::raw_convert<uint64_t>(dcp::file_to_string(*i / "size")) * 512;
			if (size == 0) {
				continue;
			}
			optional<string> vendor;
			try {
				vendor = dcp::file_to_string("/sys/block/" + name + "/device/vendor");
				boost::trim(*vendor);
			} catch (...) {}
			optional<string> model;
			try {
				model = dcp::file_to_string("/sys/block/" + name + "/device/model");
				boost::trim(*model);
			} catch (...) {}
			vector<boost::filesystem::path> mount_points;
			for (vector<pair<string, string> >::const_iterator j = mounted_devices.begin(); j != mounted_devices.end(); ++j) {
				if (boost::algorithm::starts_with(j->first, "/dev/" + name)) {
					mount_points.push_back (j->second);
				}
			}
			drives.push_back(Drive("/dev/" + name, mount_points, size, vendor, model));
			LOG_DISK_NC(drives.back().log_summary());
		}
	}

	return drives;
}


bool
Drive::unmount ()
{
	BOOST_FOREACH (boost::filesystem::path i, _mount_points) {
		int const r = umount(i.string().c_str());
		LOG_DISK("Tried to unmount %1 and got %2 and %3", i.string(), r, errno);
		if (r == -1) {
			return false;
		}
	}
	return true;
}


void
unprivileged ()
{
	uid_t ruid, euid, suid;
	if (getresuid(&ruid, &euid, &suid) == -1) {
		cerr << "getresuid() failed.\n";
		exit (EXIT_FAILURE);
	}
	seteuid (ruid);
}

PrivilegeEscalator::~PrivilegeEscalator ()
{
	unprivileged ();
}

PrivilegeEscalator::PrivilegeEscalator ()
{
	seteuid (0);
}

boost::filesystem::path
config_path ()
{
	boost::filesystem::path p;
	p /= g_get_user_config_dir ();
	p /= "dcpomatic2";
	return p;
}


void
disk_write_finished ()
{

}

