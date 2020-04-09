/*
    Copyright (C) 2019-2020 Carl Hetherington <cth@carlh.net>

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

#include "lib/disk_writer_messages.h"
#include "lib/compose.hpp"
#include "lib/exceptions.h"
#include "lib/cross.h"
#include "lib/digester.h"
#include "lib/file_log.h"
#include "lib/dcpomatic_log.h"
#include "lib/nanomsg.h"
extern "C" {
#include <lwext4/ext4_mbr.h>
#include <lwext4/ext4_fs.h>
#include <lwext4/ext4_mkfs.h>
#include <lwext4/ext4_errno.h>
#include <lwext4/ext4_debug.h>
#include <lwext4/ext4.h>
}

#ifdef DCPOMATIC_POSIX
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#ifdef DCPOMATIC_OSX
#undef nil
extern "C" {
#include <lwext4/file_dev.h>
}
#endif

#ifdef DCPOMATIC_LINUX
#include <linux/fs.h>
#include <polkit/polkit.h>
extern "C" {
#include <lwext4/file_dev.h>
}
#include <poll.h>
#endif

#ifdef DCPOMATIC_WINDOWS
extern "C" {
#include <lwext4/file_windows.h>
}
#endif

#include <glibmm.h>
#include <unistd.h>
#include <sys/types.h>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>

using std::cin;
using std::min;
using std::string;
using std::runtime_error;
using std::exception;
using boost::optional;

#ifdef DCPOMATIC_LINUX
static PolkitAuthority* polkit_authority = 0;
#endif
static uint64_t const block_size = 4096;
static Nanomsg* nanomsg = 0;

#define SHORT_TIMEOUT 100
#define LONG_TIMEOUT 2000

static
void
count (boost::filesystem::path dir, uint64_t& total_bytes)
{
	using namespace boost::filesystem;
	for (directory_iterator i = directory_iterator(dir); i != directory_iterator(); ++i) {
		if (is_directory(*i)) {
			count (*i, total_bytes);
		} else {
			total_bytes += file_size (*i);
		}
	}
}

static
string
write (boost::filesystem::path from, boost::filesystem::path to, uint64_t& total_remaining, uint64_t total)
{
	ext4_file out;
	int r = ext4_fopen(&out, to.generic_string().c_str(), "wb");
	if (r != EOK) {
		throw CopyError (String::compose("Failed to open file %1", to.generic_string()), r);
	}

	FILE* in = fopen_boost (from, "rb");
	if (!in) {
		ext4_fclose (&out);
		throw CopyError (String::compose("Failed to open file %1", from.string()), 0);
	}

	uint8_t* buffer = new uint8_t[block_size];
	Digester digester;

	uint64_t remaining = file_size (from);
	while (remaining > 0) {
		uint64_t const this_time = min(remaining, block_size);
		size_t read = fread (buffer, 1, this_time, in);
		if (read != this_time) {
			fclose (in);
			ext4_fclose (&out);
			delete[] buffer;
			throw CopyError (String::compose("Short read; expected %1 but read %2", this_time, read), 0);
		}

		digester.add (buffer, this_time);

		size_t written;
		r = ext4_fwrite (&out, buffer, this_time, &written);
		if (r != EOK) {
			fclose (in);
			ext4_fclose (&out);
			delete[] buffer;
			throw CopyError ("Write failed", r);
		}
		if (written != this_time) {
			fclose (in);
			ext4_fclose (&out);
			delete[] buffer;
			throw CopyError (String::compose("Short write; expected %1 but wrote %2", this_time, written), 0);
		}
		remaining -= this_time;
		total_remaining -= this_time;
		nanomsg->send(String::compose(DISK_WRITER_PROGRESS "\n%1\n", (1 - float(total_remaining) / total)), SHORT_TIMEOUT);
	}

	fclose (in);
	ext4_fclose (&out);
	delete[] buffer;

	return digester.get ();
}

static
string
read (boost::filesystem::path from, boost::filesystem::path to, uint64_t& total_remaining, uint64_t total)
{
	ext4_file in;
	LOG_DISK("Opening %1 for read", to.generic_string());
	int r = ext4_fopen(&in, to.generic_string().c_str(), "rb");
	if (r != EOK) {
		throw VerifyError (String::compose("Failed to open file %1", to.generic_string()), r);
	}
	LOG_DISK("Opened %1 for read", to.generic_string());

	uint8_t* buffer = new uint8_t[block_size];
	Digester digester;

	uint64_t remaining = file_size (from);
	while (remaining > 0) {
		uint64_t const this_time = min(remaining, block_size);
		size_t read;
		r = ext4_fread (&in, buffer, this_time, &read);
		if (read != this_time) {
			ext4_fclose (&in);
			delete[] buffer;
			throw VerifyError (String::compose("Short read; expected %1 but read %2", this_time, read), 0);
		}

		digester.add (buffer, this_time);
		remaining -= this_time;
		total_remaining -= this_time;
		nanomsg->send(String::compose(DISK_WRITER_PROGRESS "\n%1\n", (1 - float(total_remaining) / total)), SHORT_TIMEOUT);
	}

	ext4_fclose (&in);
	delete[] buffer;

	return digester.get ();
}


/** @param from File to copy from.
 *  @param to Directory to copy to.
 */
static
void
copy (boost::filesystem::path from, boost::filesystem::path to, uint64_t& total_remaining, uint64_t total)
{
	LOG_DISK ("Copy %1 -> %2", from.string(), to.generic_string());

	using namespace boost::filesystem;

	path const cr = to / from.filename();

	if (is_directory(from)) {
		int r = ext4_dir_mk (cr.generic_string().c_str());
		if (r != EOK) {
			throw CopyError (String::compose("Failed to create directory %1", cr.generic_string()), r);
		}

		for (directory_iterator i = directory_iterator(from); i != directory_iterator(); ++i) {
			copy (i->path(), cr, total_remaining, total);
		}
	} else {
		string const write_digest = write (from, cr, total_remaining, total);
		LOG_DISK ("Wrote %1 %2 with %3", from.string(), cr.generic_string(), write_digest);
		string const read_digest = read (from, cr, total_remaining, total);
		LOG_DISK ("Read %1 %2 with %3", from.string(), cr.generic_string(), write_digest);
		if (write_digest != read_digest) {
			throw VerifyError ("Hash of written data is incorrect", 0);
		}
	}
}


static
void
write (boost::filesystem::path dcp_path, string device)
try
{
//	ext4_dmask_set (DEBUG_ALL);

	/* We rely on static initialization for these */
	static struct ext4_fs fs;
	static struct ext4_mkfs_info info;
	info.block_size = 1024;
	info.inode_size = 128;
	info.journal = false;

#ifdef WIN32
	file_windows_name_set(device.c_str());
	struct ext4_blockdev* bd = file_windows_dev_get();
#else
	file_dev_name_set (device.c_str());
	struct ext4_blockdev* bd = file_dev_get ();
#endif

	if (!bd) {
		throw CopyError ("Failed to open drive", 0);
	}
	LOG_DISK_NC ("Opened drive");

	struct ext4_mbr_parts parts;
	parts.division[0] = 100;
	parts.division[1] = 0;
	parts.division[2] = 0;
	parts.division[3] = 0;

#ifdef DCPOMATIC_LINUX
	PrivilegeEscalator e;
#endif

	/* XXX: not sure if disk_id matters */
	int r = ext4_mbr_write (bd, &parts, 0);

	if (r) {
		throw CopyError ("Failed to write MBR", r);
	}
	LOG_DISK_NC ("Wrote MBR");

#ifdef DCPOMATIC_WINDOWS
	struct ext4_mbr_bdevs bdevs;
	r = ext4_mbr_scan (bd, &bdevs);
	if (r != EOK) {
		throw CopyError ("Failed to read MBR", r);
	}

	file_windows_partition_set (bdevs.partitions[0].part_offset, bdevs.partitions[0].part_size);
#endif

#ifdef DCPOMATIC_LINUX
	/* Re-read the partition table */
	int fd = open(device.c_str(), O_RDONLY);
	ioctl(fd, BLKRRPART, NULL);
	close(fd);
#endif

#ifdef DCPOMATIC_LINUX
	string partition = device;
	/* XXX: don't know if this logic is sensible */
	if (partition.size() > 0 && isdigit(partition[partition.length() - 1])) {
		partition += "p1";
	} else {
		partition += "1";
	}
	file_dev_name_set (partition.c_str());
	bd = file_dev_get ();
#endif

#ifdef DCPOMATIC_OSX
	string partition = device + "s1";
	file_dev_name_set (partition.c_str());
	bd = file_dev_get ();
#endif

	if (!bd) {
		throw CopyError ("Failed to open partition", 0);
	}
	LOG_DISK_NC ("Opened partition");

	nanomsg->send(DISK_WRITER_FORMATTING "\n", SHORT_TIMEOUT);

	r = ext4_mkfs(&fs, bd, &info, F_SET_EXT4);
	if (r != EOK) {
		throw CopyError ("Failed to make filesystem", r);
	}
	LOG_DISK_NC ("Made filesystem");

	r = ext4_device_register(bd, "ext4_fs");
	if (r != EOK) {
		throw CopyError ("Failed to register device", r);
	}
	LOG_DISK_NC ("Registered device");

	r = ext4_mount("ext4_fs", "/mp/", false);
	if (r != EOK) {
		throw CopyError ("Failed to mount device", r);
	}
	LOG_DISK_NC ("Mounted device");

	uint64_t total_bytes = 0;
	count (dcp_path, total_bytes);

	/* XXX: this is a hack.  We are going to "treat" every byte twice; write it, and then verify it.  Double the
	 * bytes totals so that progress works itself out (assuming write is the same speed as read).
	 */
	total_bytes *= 2;
	copy (dcp_path, "/mp", total_bytes, total_bytes);

	r = ext4_umount("/mp/");
	if (r != EOK) {
		throw CopyError ("Failed to unmount device", r);
	}

	ext4_device_unregister("ext4_fs");
	if (!nanomsg->send(DISK_WRITER_OK "\n", LONG_TIMEOUT)) {
		throw CommunicationFailedError ();
	}
} catch (CopyError& e) {
	LOG_DISK("CopyError (from write): %1 %2", e.message(), e.number().get_value_or(0));
	nanomsg->send(String::compose(DISK_WRITER_ERROR "\n%1\n%2\n", e.message(), e.number().get_value_or(0)), LONG_TIMEOUT);
} catch (VerifyError& e) {
	LOG_DISK("VerifyError (from write): %1 %2", e.message(), e.number());
	nanomsg->send(String::compose(DISK_WRITER_ERROR "\n%1\n%2\n", e.message(), e.number()), LONG_TIMEOUT);
} catch (exception& e) {
	LOG_DISK("Exception (from write): %1", e.what());
	nanomsg->send(String::compose(DISK_WRITER_ERROR "\n%1\n0\n", e.what()), LONG_TIMEOUT);
}

struct Parameters
{
	boost::filesystem::path dcp_path;
	std::string device;
};

#ifdef DCPOMATIC_LINUX
static
void
polkit_callback (GObject *, GAsyncResult* res, gpointer data)
{
	Parameters* parameters = reinterpret_cast<Parameters*> (data);
	PolkitAuthorizationResult* result = polkit_authority_check_authorization_finish (polkit_authority, res, 0);
	if (result && polkit_authorization_result_get_is_authorized(result)) {
		write (parameters->dcp_path, parameters->device);
	}
	delete parameters;
	if (result) {
		g_object_unref (result);
	}
}
#endif

bool
idle ()
try
{
	using namespace boost::algorithm;

	optional<string> s = nanomsg->receive (0);
	if (!s) {
		return true;
	}

	if (*s == DISK_WRITER_QUIT) {
		exit (EXIT_SUCCESS);
	} else if (*s == DISK_WRITER_UNMOUNT) {
		/* XXX: should do Linux polkit stuff here */
		optional<string> xml_head = nanomsg->receive (LONG_TIMEOUT);
		optional<string> xml_body = nanomsg->receive (LONG_TIMEOUT);
		if (!xml_head || !xml_body) {
			throw CommunicationFailedError ();
		}
		if (Drive(*xml_head + *xml_body).unmount()) {
			if (!nanomsg->send (DISK_WRITER_OK "\n", LONG_TIMEOUT)) {
				throw CommunicationFailedError();
			}
		} else {
			if (!nanomsg->send (DISK_WRITER_ERROR "\n", LONG_TIMEOUT)) {
				throw CommunicationFailedError();
			}
		}
	} else {
		optional<string> dcp_path = nanomsg->receive(LONG_TIMEOUT);
		optional<string> device = nanomsg->receive(LONG_TIMEOUT);
		if (!dcp_path || !device) {
			throw CommunicationFailedError();
		}

		/* Do some basic sanity checks; this is a bit belt-and-braces but it can't hurt... */

#ifdef DCPOMATIC_OSX
		if (!starts_with(*device, "/dev/disk")) {
			LOG_DISK ("Will not write to %1", *device);
			nanomsg->try_send(DISK_WRITER_ERROR "\nRefusing to write to this drive\n1\n", LONG_TIMEOUT);
			return true;
		}
#endif
#ifdef DCPOMATIC_LINUX
		if (!starts_with(*device, "/dev/sd") && !starts_with(*device, "/dev/hd")) {
			LOG_DISK ("Will not write to %1", *device);
			nanomsg->send(DISK_WRITER_ERROR "\nRefusing to write to this drive\n1\n", LONG_TIMEOUT);
			return true;
		}
#endif
#ifdef DCPOMATIC_WINDOWS
		if (!starts_with(*device, "\\\\.\\PHYSICALDRIVE")) {
			LOG_DISK ("Will not write to %1", *device);
			nanomsg->try_send(DISK_WRITER_ERROR "\nRefusing to write to this drive\n1\n", LONG_TIMEOUT);
			return true;
		}
#endif

		bool on_drive_list = false;
		bool mounted = false;
		for (auto const& i: Drive::get()) {
			if (i.device() == *device) {
				on_drive_list = true;
				mounted = i.mounted();
			}
		}

		if (!on_drive_list) {
			LOG_DISK ("Will not write to %1 as it's not recognised as a drive", *device);
			nanomsg->send(DISK_WRITER_ERROR "\nRefusing to write to this drive\n1\n", LONG_TIMEOUT);
			return true;
		}
		if (mounted) {
			LOG_DISK ("Will not write to %1 as it's mounted", *device);
			nanomsg->send(DISK_WRITER_ERROR "\nRefusing to write to this drive\n1\n", LONG_TIMEOUT);
			return true;
		}

		LOG_DISK ("Here we go writing %1 to %2", *dcp_path, *device);

#ifdef DCPOMATIC_LINUX
		polkit_authority = polkit_authority_get_sync (0, 0);
		PolkitSubject* subject = polkit_unix_process_new (getppid());
		Parameters* parameters = new Parameters;
		parameters->dcp_path = *dcp_path;
		parameters->device = *device;
		polkit_authority_check_authorization (
				polkit_authority, subject, "com.dcpomatic.write-drive", 0, POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION, 0, polkit_callback, parameters
				);
#else
		write (*dcp_path, *device);
#endif
	}

	return true;
} catch (exception& e) {
	LOG_DISK("Exception (from idle): %1", e.what());
	return true;
}

int
main ()
{
	/* XXX: this is a hack, but I expect we'll need logs and I'm not sure if there's
	 * a better place to put them.
	 */
	dcpomatic_log.reset(new FileLog(config_path() / "disk_writer.log", LogEntry::TYPE_DISK));
	LOG_DISK_NC("dcpomatic_disk_writer started");

	try {
		nanomsg = new Nanomsg (false);
	} catch (runtime_error& e) {
		LOG_DISK_NC("Could not set up nanomsg socket");
		exit (EXIT_FAILURE);
	}

	Glib::RefPtr<Glib::MainLoop> ml = Glib::MainLoop::create ();
	Glib::signal_timeout().connect(sigc::ptr_fun(&idle), 500);
	ml->run ();
}
