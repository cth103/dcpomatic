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


#include "cross.h"
#include "dcpomatic_log.h"
#include "digester.h"
#include "disk_writer_messages.h"
#include "exceptions.h"
#include "ext.h"
#include "nanomsg.h"
#include <dcp/file.h>
#include <dcp/filesystem.h>

#ifdef DCPOMATIC_LINUX
#include <linux/fs.h>
#include <sys/ioctl.h>
extern "C" {
#include <lwext4/file_dev.h>
}
#endif

#ifdef DCPOMATIC_OSX
extern "C" {
#include <lwext4/file_dev.h>
}
#endif

#ifdef DCPOMATIC_WINDOWS
extern "C" {
#include <lwext4/file_windows.h>
}
#endif

extern "C" {
#include <lwext4/ext4.h>
#include <lwext4/ext4_debug.h>
#include <lwext4/ext4_errno.h>
#include <lwext4/ext4_fs.h>
#include <lwext4/ext4_mbr.h>
#include <lwext4/ext4_mkfs.h>
}
#include <boost/filesystem.hpp>
#include <chrono>
#include <string>


using std::exception;
using std::min;
using std::string;
using std::vector;


#define SHORT_TIMEOUT 100
#define LONG_TIMEOUT 2000


/* Use quite a big block size here, as ext4's fwrite() has quite a bit of overhead */
uint64_t constexpr block_size = 4096 * 4096;


static
void
count(std::vector<boost::filesystem::path> dirs, uint64_t& total_bytes)
{
	using namespace boost::filesystem;

	for (auto dir: dirs) {
		dir = dcp::filesystem::fix_long_path(dir);
		for (auto path: directory_iterator(dir)) {
			if (is_directory(path)) {
				count({path}, total_bytes);
			} else {
				total_bytes += file_size(path);
			}
		}
	}
}


static
void
set_timestamps_to_now(boost::filesystem::path path)
{
	auto const now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	ext4_mtime_set(path.generic_string().c_str(), now);
	ext4_ctime_set(path.generic_string().c_str(), now);
	ext4_atime_set(path.generic_string().c_str(), now);
}


static
string
write(boost::filesystem::path from, boost::filesystem::path to, uint64_t& total_remaining, uint64_t total, Nanomsg* nanomsg)
{
	ext4_file out;
	int r = ext4_fopen(&out, to.generic_string().c_str(), "wb");
	if (r != EOK) {
		throw CopyError(fmt::format("Failed to open file {}", to.generic_string()), r, ext4_blockdev_errno);
	}

	dcp::File in(from, "rb");
	if (!in) {
		ext4_fclose(&out);
		throw CopyError(fmt::format("Failed to open file {}", from.string()), 0);
	}

	std::vector<uint8_t> buffer(block_size);
	Digester digester;

	int progress_frequency = 1;
	int progress_count = 0;
	uint64_t remaining = file_size(from);
	while (remaining > 0) {
		uint64_t const this_time = min(remaining, block_size);
		size_t read = in.read(buffer.data(), 1, this_time);
		if (read != this_time) {
			ext4_fclose(&out);
			throw CopyError(fmt::format("Short read; expected {} but read {}", this_time, read), 0, ext4_blockdev_errno);
		}

		digester.add(buffer.data(), this_time);

		size_t written;
		r = ext4_fwrite(&out, buffer.data(), this_time, &written);
		if (r != EOK) {
			ext4_fclose(&out);
			throw CopyError("Write failed", r, ext4_blockdev_errno);
		}
		if (written != this_time) {
			ext4_fclose(&out);
			throw CopyError(fmt::format("Short write; expected {} but wrote {}", this_time, written), 0, ext4_blockdev_errno);
		}
		remaining -= this_time;
		total_remaining -= this_time;

		++progress_count;
		if ((progress_count % progress_frequency) == 0 && nanomsg) {
			DiskWriterBackEndResponse::copy_progress(1 - float(total_remaining) / total).write_to_nanomsg(*nanomsg, SHORT_TIMEOUT);
		}
	}

	ext4_fclose(&out);

	set_timestamps_to_now(to);

	return digester.get();
}


static
string
read(boost::filesystem::path from, boost::filesystem::path to, uint64_t& total_remaining, uint64_t total, Nanomsg* nanomsg)
{
	ext4_file in;
	LOG_DISK("Opening {} for read", to.generic_string());
	int r = ext4_fopen(&in, to.generic_string().c_str(), "rb");
	if (r != EOK) {
		throw VerifyError(fmt::format("Failed to open file {}", to.generic_string()), r);
	}
	LOG_DISK("Opened {} for read", to.generic_string());

	std::vector<uint8_t> buffer(block_size);
	Digester digester;

	uint64_t remaining = file_size(from);
	while (remaining > 0) {
		uint64_t const this_time = min(remaining, block_size);
		size_t read;
		r = ext4_fread(&in, buffer.data(), this_time, &read);
		if (read != this_time) {
			ext4_fclose(&in);
			throw VerifyError(fmt::format("Short read; expected {} but read {}", this_time, read), 0);
		}

		digester.add(buffer.data(), this_time);
		remaining -= this_time;
		total_remaining -= this_time;
		if (nanomsg) {
			DiskWriterBackEndResponse::verify_progress(1 - float(total_remaining) / total).write_to_nanomsg(*nanomsg, SHORT_TIMEOUT);
		}
	}

	ext4_fclose(&in);

	return digester.get();
}


class CopiedFile
{
public:
	CopiedFile(boost::filesystem::path from_, boost::filesystem::path to_, string write_digest_)
		: from(from_)
		, to(to_)
		, write_digest(write_digest_)
	{}

	boost::filesystem::path from;
	boost::filesystem::path to;
	/** digest calculated from data as it was read from the source during write */
	string write_digest;
};


/** @param from File to copy from.
 *  @param to Directory to copy to.
 */
static
void
copy(boost::filesystem::path from, boost::filesystem::path to, uint64_t& total_remaining, uint64_t total, vector<CopiedFile>& copied_files, Nanomsg* nanomsg)
{
	LOG_DISK("Copy {} -> {}", from.string(), to.generic_string());
	from = dcp::filesystem::fix_long_path(from);

	using namespace boost::filesystem;

	path const cr = to / from.filename();

	if (is_directory(from)) {
		int r = ext4_dir_mk(cr.generic_string().c_str());
		if (r != EOK) {
			throw CopyError(fmt::format("Failed to create directory {}", cr.generic_string()), r, ext4_blockdev_errno);
		}
		set_timestamps_to_now(cr);

		for (auto i: directory_iterator(from)) {
			copy(i.path(), cr, total_remaining, total, copied_files, nanomsg);
		}
	} else {
		string const write_digest = write(from, cr, total_remaining, total, nanomsg);
		LOG_DISK("Wrote {} {} with {}", from.string(), cr.generic_string(), write_digest);
		copied_files.push_back(CopiedFile(from, cr, write_digest));
	}
}


static
void
verify(vector<CopiedFile> const& copied_files, uint64_t total, Nanomsg* nanomsg)
{
	uint64_t total_remaining = total;
	for (auto const& i: copied_files) {
		string const read_digest = read(i.from, i.to, total_remaining, total, nanomsg);
		LOG_DISK("Read {} {} was {} on write, now {}", i.from.string(), i.to.generic_string(), i.write_digest, read_digest);
		if (read_digest != i.write_digest) {
			throw VerifyError("Hash of written data is incorrect", 0);
		}
	}
}


static
void
format_progress(void* context, float progress)
{
	if (context) {
		DiskWriterBackEndResponse::format_progress(progress).write_to_nanomsg(*reinterpret_cast<Nanomsg*>(context), SHORT_TIMEOUT);
	}
}


void
#ifdef DCPOMATIC_WINDOWS
dcpomatic::write(vector<boost::filesystem::path> dcp_paths, string device, string, Nanomsg* nanomsg)
#else
dcpomatic::write(vector<boost::filesystem::path> dcp_paths, string device, string posix_partition, Nanomsg* nanomsg)
#endif
try
{
	ext4_dmask_set(DEBUG_ALL);

	struct ext4_fs fs;
	fs.read_only = false;
	fs.bdev = nullptr;
	fs.last_inode_bg_id = 0;
	fs.jbd_fs = nullptr;
	fs.jbd_journal = nullptr;
	fs.curr_trans = nullptr;
	struct ext4_mkfs_info info;
	info.len = 0;
	info.block_size = 4096;
	info.blocks_per_group = 0;
	info.inode_size = 128;
	info.inodes = 0;
	info.journal_blocks = 0;
	info.dsc_size = 0;
	for (int i = 0; i < UUID_SIZE; ++i) {
		info.uuid[i] = rand() & 0xff;
	}
	info.journal = false;
	info.label = nullptr;

#ifdef WIN32
	file_windows_name_set(device.c_str());
	struct ext4_blockdev* bd = file_windows_dev_get();
#else
	file_dev_name_set(device.c_str());
	struct ext4_blockdev* bd = file_dev_get();
#endif

	if (!bd) {
		throw CopyError("Failed to open drive", 0, ext4_blockdev_errno);
	}
	LOG_DISK_NC("Opened drive");

	struct ext4_mbr_parts parts;
	parts.division[0] = 100;
	parts.division[1] = 0;
	parts.division[2] = 0;
	parts.division[3] = 0;

	/* XXX: not sure if disk_id matters */
	int r = ext4_mbr_write(bd, &parts, 0);
	if (r) {
		throw CopyError("Failed to write MBR", r, ext4_blockdev_errno);
	}
	LOG_DISK_NC("Wrote MBR");

	struct ext4_mbr_bdevs bdevs;
	r = ext4_mbr_scan(bd, &bdevs);
	if (r != EOK) {
		throw CopyError("Failed to read MBR", r, ext4_blockdev_errno);
	}

#ifdef DCPOMATIC_LINUX
	/* Re-read the partition table */
	int fd = open(device.c_str(), O_RDONLY);
	ioctl(fd, BLKRRPART, NULL);
	close(fd);
#endif

	LOG_DISK("Writing to partition at {} size {}; bd part size is {}", bdevs.partitions[0].part_offset, bdevs.partitions[0].part_size, bd->part_size);

#ifdef DCPOMATIC_WINDOWS
	file_windows_partition_set(bdevs.partitions[0].part_offset, bdevs.partitions[0].part_size);
#else
	file_dev_name_set(posix_partition.c_str());

	/* On macOS (at least) if you try to write to a drive that is sleeping the ext4_mkfs call
	 * below is liable to return EIO because it can't open the device.  Try to work around that
	 * here by opening and closing the device, waiting 5 seconds if it fails.
	 */
	int wake = open(posix_partition.c_str(), O_RDWR);
	if (wake == -1) {
		dcpomatic_sleep_seconds(5);
	} else {
		close(wake);
	}

	bd = file_dev_get();
#endif

	if (!bd) {
		throw CopyError("Failed to open partition", 0, ext4_blockdev_errno);
	}
	LOG_DISK_NC("Opened partition");

	r = ext4_mkfs(&fs, bd, &info, F_SET_EXT2, format_progress, nanomsg);
	if (r != EOK) {
		throw CopyError("Failed to make filesystem", r, ext4_blockdev_errno);
	}
	LOG_DISK_NC("Made filesystem");

	r = ext4_device_register(bd, "ext4_fs");
	if (r != EOK) {
		throw CopyError("Failed to register device", r, ext4_blockdev_errno);
	}
	LOG_DISK_NC("Registered device");

	r = ext4_mount("ext4_fs", "/mp/", false);
	if (r != EOK) {
		throw CopyError("Failed to mount device", r, ext4_blockdev_errno);
	}
	LOG_DISK_NC("Mounted device");

	uint64_t total_bytes = 0;
	count(dcp_paths, total_bytes);

	uint64_t total_remaining = total_bytes;
	vector<CopiedFile> copied_files;
	for (auto dcp_path: dcp_paths) {
		copy(dcp_path, "/mp", total_remaining, total_bytes, copied_files, nanomsg);
	}

	/* Unmount and re-mount to make sure the write has finished */
	r = ext4_umount("/mp/");
	if (r != EOK) {
		throw CopyError("Failed to unmount device", r, ext4_blockdev_errno);
	}
	r = ext4_mount("ext4_fs", "/mp/", false);
	if (r != EOK) {
		throw CopyError("Failed to mount device", r, ext4_blockdev_errno);
	}
	LOG_DISK_NC("Re-mounted device");

	verify(copied_files, total_bytes, nanomsg);

	r = ext4_umount("/mp/");
	if (r != EOK) {
		throw CopyError("Failed to unmount device", r, ext4_blockdev_errno);
	}

	ext4_device_unregister("ext4_fs");
	if (nanomsg && !DiskWriterBackEndResponse::ok().write_to_nanomsg(*nanomsg, LONG_TIMEOUT)) {
		throw CommunicationFailedError();
	}

	disk_write_finished();
} catch (CopyError& e) {
	LOG_DISK("CopyError (from write): {} {} {}", e.message(), e.ext4_number().get_value_or(0), e.platform_number().get_value_or(0));
	if (nanomsg) {
		DiskWriterBackEndResponse::error(e.message(), e.ext4_number().get_value_or(0), e.platform_number().get_value_or(0)).write_to_nanomsg(*nanomsg, LONG_TIMEOUT);
	}
} catch (VerifyError& e) {
	LOG_DISK("VerifyError (from write): {} {}", e.message(), e.number());
	if (nanomsg) {
		DiskWriterBackEndResponse::error(e.message(), e.number(), 0).write_to_nanomsg(*nanomsg, LONG_TIMEOUT);
	}
} catch (exception& e) {
	LOG_DISK("Exception (from write): {}", e.what());
	if (nanomsg) {
		DiskWriterBackEndResponse::error(e.what(), 0, 0).write_to_nanomsg(*nanomsg, LONG_TIMEOUT);
	}
}


