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

/** @file  src/lib/cross.h
 *  @brief Cross-platform compatibility code.
 */

#ifndef DCPOMATIC_CROSS_H
#define DCPOMATIC_CROSS_H

#ifdef DCPOMATIC_OSX
#include <IOKit/pwr_mgt/IOPMLib.h>
#endif
#include <boost/filesystem.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/optional.hpp>
#include <boost/function.hpp>

#ifdef DCPOMATIC_WINDOWS
#define WEXITSTATUS(w) (w)
#endif

class Log;
struct AVIOContext;

void dcpomatic_sleep_seconds (int);
void dcpomatic_sleep_milliseconds (int);
extern std::string cpu_info ();
extern void run_ffprobe (boost::filesystem::path, boost::filesystem::path);
extern std::list<std::pair<std::string, std::string> > mount_info ();
extern boost::filesystem::path openssl_path ();
#ifdef DCPOMATIC_DISK
extern boost::filesystem::path disk_writer_path ();
#endif
#ifdef DCPOMATIC_WINDOWS
extern void maybe_open_console ();
#endif
extern boost::filesystem::path shared_path ();
extern FILE * fopen_boost (boost::filesystem::path, std::string);
extern int dcpomatic_fseek (FILE *, int64_t, int);
extern void start_batch_converter ();
extern void start_player ();
extern uint64_t thread_id ();
extern int avio_open_boost (AVIOContext** s, boost::filesystem::path file, int flags);
extern boost::filesystem::path home_directory ();
extern std::string command_and_read (std::string cmd);
extern bool running_32_on_64 ();
extern void unprivileged ();
extern boost::filesystem::path config_path ();
extern boost::filesystem::path directory_containing_executable ();

class PrivilegeEscalator
{
public:
	PrivilegeEscalator ();
	~PrivilegeEscalator ();
};

/** @class Waker
 *  @brief A class which tries to keep the computer awake on various operating systems.
 *
 *  Create a Waker to prevent sleep, and call nudge() every so often (every minute or so).
 *  Destroy the Waker to allow sleep again.
 */
class Waker
{
public:
	Waker ();
	~Waker ();

	void nudge ();

private:
	boost::mutex _mutex;
#ifdef DCPOMATIC_OSX
	IOPMAssertionID _assertion_id;
#endif
};

class Drive
{
public:
	Drive (std::string device, std::vector<boost::filesystem::path> mount_points, uint64_t size, boost::optional<std::string> vendor, boost::optional<std::string> model)
		: _device(device)
		, _mount_points(mount_points)
		, _size(size)
		, _vendor(vendor)
		, _model(model)
	{}

	explicit Drive (std::string);

	std::string as_xml () const;

	std::string description () const;

	std::string device () const {
		return _device;
	}

	bool mounted () const {
		return !_mount_points.empty();
	}

	std::string log_summary () const;

	/** Unmount any mounted partitions on a drive.
	 *  @return true on success, false on failure.
	 */
	bool unmount ();

	static std::vector<Drive> get ();

private:
	std::string _device;
	std::vector<boost::filesystem::path> _mount_points;
	/** size in bytes */
	uint64_t _size;
	boost::optional<std::string> _vendor;
	boost::optional<std::string> _model;
};

void disk_write_finished ();

#endif
