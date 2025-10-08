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


#include "config.h"
#include "cross.h"
#include "dcpomatic_log.h"
#include "dcpomatic_log.h"
#include "exceptions.h"
#include "log.h"
#include "util.h"
#include <dcp/filesystem.h>
#include <dcp/raw_convert.h>
#include <dcp/warnings.h>
#include <glib.h>
#include <boost/algorithm/string.hpp>
#if BOOST_VERSION >= 106100
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


using std::cerr;
using std::cout;
using std::ifstream;
using std::list;
using std::make_pair;
using std::pair;
using std::string;
using std::vector;
using boost::optional;


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
resources_path ()
{
	auto installed = directory_containing_executable().parent_path() / "share" / "dcpomatic2";
	if (boost::filesystem::exists(installed)) {
		return installed;
	}

	/* Fallback for running from the source tree */
	return directory_containing_executable().parent_path().parent_path().parent_path();
}


boost::filesystem::path
libdcp_resources_path ()
{
	if (running_tests) {
		return directory_containing_executable();
	}

	if (auto appdir = getenv("APPDIR")) {
		return boost::filesystem::path(appdir) / "usr" / "share" / "libdcp";
	}
	return dcp::filesystem::canonical(LINUX_SHARE_PREFIX) / "libdcp";
}


void
run_ffprobe(boost::filesystem::path content, boost::filesystem::path out, bool err, string args)
{
	string const redirect = err ? "2>" : ">";
	auto const ffprobe = fmt::format("ffprobe {} \"{}\" {} \"{}\"", args.empty() ? " " : args, content.string(), redirect, out.string());
	LOG_GENERAL (N_("Probing with {}"), ffprobe);
	int const r = system (ffprobe.c_str());
	if (r == -1 || (WIFEXITED(r) && WEXITSTATUS(r) != 0)) {
		LOG_GENERAL (N_("Could not run ffprobe (system returned {}"), r);
	}
}


list<pair<string, string>>
mount_info ()
{
	list<pair<string, string>> m;

	auto f = setmntent ("/etc/mtab", "r");
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
directory_containing_executable ()
{
#if BOOST_VERSION >= 106100
	return boost::dll::program_location().parent_path();
#else
	char buffer[PATH_MAX];
	ssize_t N = readlink ("/proc/self/exe", buffer, PATH_MAX);
	return boost::filesystem::path(string(buffer, N)).parent_path();
#endif
}


boost::filesystem::path
openssl_path ()
{
	auto p = directory_containing_executable() / "dcpomatic2_openssl";
	if (dcp::filesystem::is_regular_file(p)) {
		return p;
	}

	return "dcpomatic2_openssl";
}


#ifdef DCPOMATIC_DISK
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


Waker::Waker(Reason)
{

}


Waker::~Waker ()
{

}


void
start_tool (string executable)
{
	auto batch = directory_containing_executable() / executable;

	pid_t pid = fork ();
	if (pid == 0) {
		int const r = system (batch.string().c_str());
		exit (WEXITSTATUS (r));
	}
}


void
start_batch_converter ()
{
	start_tool ("dcpomatic2_batch");
}


void
start_player ()
{
	start_tool ("dcpomatic2_player");
}


static
vector<pair<string, string>>
get_mounts (string prefix)
{
	vector<pair<string, string>> mounts;

	std::ifstream f("/proc/mounts");
	string line;
	while (f.good()) {
		getline(f, line);
		vector<string> bits;
		boost::algorithm::split (bits, line, boost::is_any_of(" "));
		if (bits.size() > 1 && boost::algorithm::starts_with(bits[0], prefix)) {
			boost::algorithm::replace_all (bits[1], "\\040", " ");
			mounts.push_back(make_pair(bits[0], bits[1]));
			LOG_DISK("Found mounted device {} from prefix {}", bits[0], prefix);
		}
	}

	return mounts;
}


vector<Drive>
Drive::get ()
{
	vector<Drive> drives;

	using namespace boost::filesystem;
	auto mounted_devices = get_mounts("/dev/");

	for (auto i: directory_iterator("/sys/block")) {
		string const name = i.path().filename().string();
		path device_type_file("/sys/block/" + name + "/device/type");
		optional<string> device_type;
		if (exists(device_type_file)) {
			device_type = dcp::file_to_string (device_type_file);
			boost::trim(*device_type);
		}
		/* Device type 5 is "SCSI_TYPE_ROM" in blkdev.h; seems usually to be a CD/DVD drive */
		if (!boost::algorithm::starts_with(name, "loop") && (!device_type || *device_type != "5")) {
			uint64_t const size = dcp::raw_convert<uint64_t>(dcp::file_to_string(i / "size")) * 512;
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
			for (auto const& j: mounted_devices) {
				if (boost::algorithm::starts_with(j.first, "/dev/" + name)) {
					mount_points.push_back (j.second);
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
	for (auto i: _mount_points) {
		int const r = umount(i.string().c_str());
		LOG_DISK("Tried to unmount {} and got {} and {}", i.string(), r, errno);
		if (r == -1) {
			return false;
		}
	}
	return true;
}


boost::filesystem::path
config_path (optional<string> version)
{
	boost::filesystem::path p;
	p /= g_get_user_config_dir ();
	p /= "dcpomatic2";
	if (version) {
		p /= *version;
	}
	return p;
}


void
disk_write_finished ()
{

}

bool
show_in_file_manager (boost::filesystem::path dir, boost::filesystem::path)
{
	int r = system ("which nautilus");
	if (WEXITSTATUS(r) == 0) {
		r = system (fmt::format("nautilus \"{}\"", dir.string()).c_str());
		return static_cast<bool>(WEXITSTATUS(r));
	} else {
		int r = system ("which konqueror");
		if (WEXITSTATUS(r) == 0) {
			r = system (fmt::format("konqueror \"{}\"", dir.string()).c_str());
			return static_cast<bool>(WEXITSTATUS(r));
		}
	}

	return true;
}

