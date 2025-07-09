/*
    Copyright (C) 2019-2021 Carl Hetherington <cth@carlh.net>

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


#include "lib/compose.hpp"
#include "lib/cross.h"
#include "lib/dcpomatic_log.h"
#include "lib/digester.h"
#include "lib/disk_writer_messages.h"
#include "lib/exceptions.h"
#include "lib/ext.h"
#include "lib/file_log.h"
#include "lib/state.h"
#include "lib/nanomsg.h"
#include "lib/util.h"
#include "lib/version.h"
#include <dcp/warnings.h>

#ifdef DCPOMATIC_POSIX
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#ifdef DCPOMATIC_OSX
#include "lib/stdout_log.h"
#undef nil
extern "C" {
#include <lwext4/file_dev.h>
}
#include <unistd.h>
#include <xpc/xpc.h>
#endif

#ifdef DCPOMATIC_LINUX
#include <polkit/polkit.h>
#include <poll.h>
#endif

#ifdef DCPOMATIC_WINDOWS
extern "C" {
#include <lwext4/file_windows.h>
}
#endif

LIBDCP_DISABLE_WARNINGS
#include <glibmm.h>
LIBDCP_ENABLE_WARNINGS

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
using std::vector;
using boost::optional;


#define SHORT_TIMEOUT 100
#define LONG_TIMEOUT 2000


#ifdef DCPOMATIC_LINUX
static PolkitAuthority* polkit_authority = nullptr;
#endif
static Nanomsg* nanomsg = nullptr;


#ifdef DCPOMATIC_LINUX
void
polkit_callback (GObject *, GAsyncResult* res, gpointer data)
{
	auto parameters = reinterpret_cast<std::pair<std::function<void ()>, std::function<void ()>>*> (data);
	GError* error = nullptr;
	auto result = polkit_authority_check_authorization_finish (polkit_authority, res, &error);
	bool failed = false;

	if (error) {
		LOG_DISK("polkit authority check failed (check_authorization_finish failed with {})", error->message);
		failed = true;
	} else {
		if (polkit_authorization_result_get_is_authorized(result)) {
			parameters->first();
		} else {
			failed = true;
			if (polkit_authorization_result_get_is_challenge(result)) {
				LOG_DISK_NC("polkit authority check failed (challenge)");
			} else {
				LOG_DISK_NC("polkit authority check failed (not authorized)");
			}
		}
	}

	if (failed) {
		parameters->second();
	}

	delete parameters;

	if (result) {
		g_object_unref (result);
	}
}
#endif


#ifdef DCPOMATIC_LINUX
void request_privileges (string action, std::function<void ()> granted, std::function<void ()> denied)
#else
void request_privileges (string, std::function<void ()> granted, std::function<void ()>)
#endif
{
#ifdef DCPOMATIC_LINUX
	polkit_authority = polkit_authority_get_sync (0, 0);
	auto subject = polkit_unix_process_new_for_owner (getppid(), 0, -1);

	auto parameters = new std::pair<std::function<void ()>, std::function<void ()>>(granted, denied);
	polkit_authority_check_authorization (
		polkit_authority, subject, action.c_str(), 0, POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION, 0, polkit_callback, parameters
		);
#else
	granted ();
#endif
}


bool
idle ()
try
{
	using namespace boost::algorithm;

	auto s = nanomsg->receive (0);
	if (!s) {
		return true;
	}

	LOG_DISK("Writer receives command: {}", *s);

	if (*s == DISK_WRITER_QUIT) {
		exit (EXIT_SUCCESS);
	} else if (*s == DISK_WRITER_PING) {
		DiskWriterBackEndResponse::pong().write_to_nanomsg(*nanomsg, LONG_TIMEOUT);
	} else if (*s == DISK_WRITER_UNMOUNT) {
		auto xml_head = nanomsg->receive (LONG_TIMEOUT);
		auto xml_body = nanomsg->receive (LONG_TIMEOUT);
		if (!xml_head || !xml_body) {
			LOG_DISK_NC("Failed to receive unmount request");
			throw CommunicationFailedError ();
		}
		auto xml = *xml_head + *xml_body;
		request_privileges (
			"com.dcpomatic.write-drive",
			[xml]() {
				bool const success = Drive(xml).unmount();
				bool sent_reply = false;
				if (success) {
					sent_reply = DiskWriterBackEndResponse::ok().write_to_nanomsg(*nanomsg, LONG_TIMEOUT);
				} else {
					sent_reply = DiskWriterBackEndResponse::error("Could not unmount drive", 1, 0).write_to_nanomsg(*nanomsg, LONG_TIMEOUT);
				}
				if (!sent_reply) {
					LOG_DISK_NC("CommunicationFailedError in unmount_finished");
					throw CommunicationFailedError ();
				}
			},
			[]() {
				if (!DiskWriterBackEndResponse::error("Could not get permission to unmount drive", 1, 0).write_to_nanomsg(*nanomsg, LONG_TIMEOUT)) {
					LOG_DISK_NC("CommunicationFailedError in unmount_finished");
					throw CommunicationFailedError ();
				}
			});
	} else if (*s == DISK_WRITER_WRITE) {
		auto device_opt = nanomsg->receive (LONG_TIMEOUT);
		if (!device_opt) {
			LOG_DISK_NC("Failed to receive write request");
			throw CommunicationFailedError();
		}
		auto device = *device_opt;

		vector<boost::filesystem::path> dcp_paths;
		while (true) {
			auto dcp_path_opt = nanomsg->receive (LONG_TIMEOUT);
			if (!dcp_path_opt) {
				LOG_DISK_NC("Failed to receive write request");
				throw CommunicationFailedError();
			}
			if (*dcp_path_opt != "") {
				dcp_paths.push_back(*dcp_path_opt);
			} else {
				break;
			}
		}

		/* Do some basic sanity checks; this is a bit belt-and-braces but it can't hurt... */

#ifdef DCPOMATIC_OSX
		if (!starts_with(device, "/dev/disk")) {
			LOG_DISK ("Will not write to {}", device);
			DiskWriterBackEndResponse::error("Refusing to write to this drive", 1, 0).write_to_nanomsg(*nanomsg, LONG_TIMEOUT);
			return true;
		}
#endif
#ifdef DCPOMATIC_LINUX
		if (!starts_with(device, "/dev/sd") && !starts_with(device, "/dev/hd")) {
			LOG_DISK ("Will not write to {}", device);
			DiskWriterBackEndResponse::error("Refusing to write to this drive", 1, 0).write_to_nanomsg(*nanomsg, LONG_TIMEOUT);
			return true;
		}
#endif
#ifdef DCPOMATIC_WINDOWS
		if (!starts_with(device, "\\\\.\\PHYSICALDRIVE")) {
			LOG_DISK ("Will not write to {}", device);
			DiskWriterBackEndResponse::error("Refusing to write to this drive", 1, 0).write_to_nanomsg(*nanomsg, LONG_TIMEOUT);
			return true;
		}
#endif

		bool on_drive_list = false;
		bool mounted = false;
		for (auto const& i: Drive::get()) {
			if (i.device() == device) {
				on_drive_list = true;
				mounted = i.mounted();
			}
		}

		if (!on_drive_list) {
			LOG_DISK ("Will not write to {} as it's not recognised as a drive", device);
			DiskWriterBackEndResponse::error("Refusing to write to this drive", 1, 0).write_to_nanomsg(*nanomsg, LONG_TIMEOUT);
			return true;
		}
		if (mounted) {
			LOG_DISK ("Will not write to {} as it's mounted", device);
			DiskWriterBackEndResponse::error("Refusing to write to this drive", 1, 0).write_to_nanomsg(*nanomsg, LONG_TIMEOUT);
			return true;
		}

		LOG_DISK("Here we go writing these to {}", device);
		for (auto dcp: dcp_paths) {
			LOG_DISK("  {}", dcp.string());
		}

		request_privileges (
			"com.dcpomatic.write-drive",
			[dcp_paths, device]() {
#if defined(DCPOMATIC_LINUX)
				auto posix_partition = device;
				/* XXX: don't know if this logic is sensible */
				if (posix_partition.size() > 0 && isdigit(posix_partition[posix_partition.length() - 1])) {
					posix_partition += "p1";
				} else {
					posix_partition += "1";
				}
				dcpomatic::write (dcp_paths, device, posix_partition, nanomsg);
#elif defined(DCPOMATIC_OSX)
				auto fast_device = boost::algorithm::replace_first_copy (device, "/dev/disk", "/dev/rdisk");
				dcpomatic::write (dcp_paths, fast_device, fast_device + "s1", nanomsg);
#elif defined(DCPOMATIC_WINDOWS)
				dcpomatic::write (dcp_paths, device, "", nanomsg);
#endif
			},
			[]() {
				if (nanomsg) {
					DiskWriterBackEndResponse::error("Could not obtain authorization to write to the drive", 1, 0).write_to_nanomsg(*nanomsg, LONG_TIMEOUT);
				}
			});
	}

	return true;
} catch (exception& e) {
	LOG_DISK("Exception (from idle): {}", e.what());
	return true;
}

int
main ()
{
	dcpomatic_setup_path_encoding();

#ifdef DCPOMATIC_OSX
	/* On macOS this is running as root, so config_path() will be somewhere in root's
	 * home.  Instead, just write to stdout as the macOS process control stuff will
	 * redirect this to a file in /var/log
	 */
	dcpomatic_log.reset(new StdoutLog(LogEntry::TYPE_DISK));
	LOG_DISK("dcpomatic_disk_writer {} started uid={} euid={}", dcpomatic_git_commit, getuid(), geteuid());
#else
	/* XXX: this is a hack, but I expect we'll need logs and I'm not sure if there's
	 * a better place to put them.
	 */
	dcpomatic_log.reset(new FileLog(State::write_path("disk_writer.log"), LogEntry::TYPE_DISK));
	LOG_DISK_NC("dcpomatic_disk_writer started");
#endif

#ifdef DCPOMATIC_OSX
	/* I *think* this consumes the notifyd event that we used to start the process, so we only
	 * get started once per notification.
	 */
        xpc_set_event_stream_handler("com.apple.notifyd.matching", DISPATCH_TARGET_QUEUE_DEFAULT, ^(xpc_object_t) {});
#endif

	try {
		nanomsg = new Nanomsg (false);
	} catch (runtime_error& e) {
		LOG_DISK_NC("Could not set up nanomsg socket");
		exit (EXIT_FAILURE);
	}

	LOG_DISK_NC("Entering main loop");
	auto ml = Glib::MainLoop::create ();
	Glib::signal_timeout().connect(sigc::ptr_fun(&idle), 500);
	ml->run ();
}
