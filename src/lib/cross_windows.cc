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
#include "dcpomatic_assert.h"
#include <dcp/raw_convert.h>
#include <glib.h>
extern "C" {
#include <libavformat/avio.h>
}
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
#include <windows.h>
#include <winternl.h>
#include <winioctl.h>
#include <ntdddisk.h>
#include <setupapi.h>
#undef DATADIR
#include <shlwapi.h>
#include <shellapi.h>
#include <fcntl.h>
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

/** @param s Number of seconds to sleep for */
void
dcpomatic_sleep_seconds (int s)
{
	Sleep (s * 1000);
}

void
dcpomatic_sleep_milliseconds (int ms)
{
	Sleep (ms);
}

/** @return A string of CPU information (model name etc.) */
string
cpu_info ()
{
	string info;

	HKEY key;
	if (RegOpenKeyEx (HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &key) != ERROR_SUCCESS) {
		return info;
	}

	DWORD type;
	DWORD data;
	if (RegQueryValueEx (key, L"ProcessorNameString", 0, &type, 0, &data) != ERROR_SUCCESS) {
		return info;
	}

	if (type != REG_SZ) {
		return info;
	}

	wstring value (data / sizeof (wchar_t), L'\0');
	if (RegQueryValueEx (key, L"ProcessorNameString", 0, 0, reinterpret_cast<LPBYTE> (&value[0]), &data) != ERROR_SUCCESS) {
		RegCloseKey (key);
		return info;
	}

	info = string (value.begin(), value.end());

	RegCloseKey (key);

	return info;
}

void
run_ffprobe (boost::filesystem::path content, boost::filesystem::path out)
{
	SECURITY_ATTRIBUTES security;
	security.nLength = sizeof (security);
	security.bInheritHandle = TRUE;
	security.lpSecurityDescriptor = 0;

	HANDLE child_stderr_read;
	HANDLE child_stderr_write;
	if (!CreatePipe (&child_stderr_read, &child_stderr_write, &security, 0)) {
		LOG_ERROR_NC ("ffprobe call failed (could not CreatePipe)");
		return;
	}

	wchar_t dir[512];
	GetModuleFileName (GetModuleHandle (0), dir, sizeof (dir));
	PathRemoveFileSpec (dir);
	SetCurrentDirectory (dir);

	STARTUPINFO startup_info;
	ZeroMemory (&startup_info, sizeof (startup_info));
	startup_info.cb = sizeof (startup_info);
	startup_info.hStdError = child_stderr_write;
	startup_info.dwFlags |= STARTF_USESTDHANDLES;

	wchar_t command[512];
	wcscpy (command, L"ffprobe.exe \"");

	wchar_t file[512];
	MultiByteToWideChar (CP_UTF8, 0, content.string().c_str(), -1, file, sizeof(file));
	wcscat (command, file);

	wcscat (command, L"\"");

	PROCESS_INFORMATION process_info;
	ZeroMemory (&process_info, sizeof (process_info));
	if (!CreateProcess (0, command, 0, 0, TRUE, CREATE_NO_WINDOW, 0, 0, &startup_info, &process_info)) {
		LOG_ERROR_NC (N_("ffprobe call failed (could not CreateProcess)"));
		return;
	}

	FILE* o = fopen_boost (out, "w");
	if (!o) {
		LOG_ERROR_NC (N_("ffprobe call failed (could not create output file)"));
		return;
	}

	CloseHandle (child_stderr_write);

	while (true) {
		char buffer[512];
		DWORD read;
		if (!ReadFile(child_stderr_read, buffer, sizeof(buffer), &read, 0) || read == 0) {
			break;
		}
		fwrite (buffer, read, 1, o);
	}

	fclose (o);

	WaitForSingleObject (process_info.hProcess, INFINITE);
	CloseHandle (process_info.hProcess);
	CloseHandle (process_info.hThread);
	CloseHandle (child_stderr_read);
}

list<pair<string, string> >
mount_info ()
{
	list<pair<string, string> > m;
	return m;
}

static boost::filesystem::path
executable_path ()
{
	return boost::dll::program_location().parent_path();
}

boost::filesystem::path
shared_path ()
{
	return executable_path().parent_path();
}

boost::filesystem::path
openssl_path ()
{
	return executable_path() / "openssl.exe";
}

#ifdef DCPOMATIC_DISK
boost::filesystem::path
disk_writer_path ()
{
	return executable_path() / "dcpomatic2_disk_writer.exe";
}
#endif

/* Apparently there is no way to create an ofstream using a UTF-8
   filename under Windows.  We are hence reduced to using fopen
   with this wrapper.
*/
FILE *
fopen_boost (boost::filesystem::path p, string t)
{
        wstring w (t.begin(), t.end());
	/* c_str() here should give a UTF-16 string */
        return _wfopen (p.c_str(), w.c_str ());
}

int
dcpomatic_fseek (FILE* stream, int64_t offset, int whence)
{
	return _fseeki64 (stream, offset, whence);
}

void
Waker::nudge ()
{
	boost::mutex::scoped_lock lm (_mutex);
	SetThreadExecutionState (ES_SYSTEM_REQUIRED);
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

	STARTUPINFO startup_info;
	ZeroMemory (&startup_info, sizeof (startup_info));
	startup_info.cb = sizeof (startup_info);

	PROCESS_INFORMATION process_info;
	ZeroMemory (&process_info, sizeof (process_info));

	wchar_t cmd[512];
	MultiByteToWideChar (CP_UTF8, 0, batch.string().c_str(), -1, cmd, sizeof(cmd));
	CreateProcess (0, cmd, 0, 0, FALSE, 0, 0, 0, &startup_info, &process_info);
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
	return (uint64_t) GetCurrentThreadId ();
}

static string
wchar_to_utf8 (wchar_t const * s)
{
	int const length = (wcslen(s) + 1) * 2;
	char* utf8 = new char[length];
	WideCharToMultiByte (CP_UTF8, 0, s, -1, utf8, length, 0, 0);
	string u (utf8);
	delete[] utf8;
	return u;
}

int
avio_open_boost (AVIOContext** s, boost::filesystem::path file, int flags)
{
	return avio_open (s, wchar_to_utf8(file.c_str()).c_str(), flags);
}

void
maybe_open_console ()
{
	if (Config::instance()->win32_console ()) {
		AllocConsole();

		HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
		int hCrt = _open_osfhandle((intptr_t) handle_out, _O_TEXT);
		FILE* hf_out = _fdopen(hCrt, "w");
		setvbuf(hf_out, NULL, _IONBF, 1);
		*stdout = *hf_out;

		HANDLE handle_in = GetStdHandle(STD_INPUT_HANDLE);
		hCrt = _open_osfhandle((intptr_t) handle_in, _O_TEXT);
		FILE* hf_in = _fdopen(hCrt, "r");
		setvbuf(hf_in, NULL, _IONBF, 128);
		*stdin = *hf_in;
	}
}

boost::filesystem::path
home_directory ()
{
	return boost::filesystem::path(getenv("HOMEDRIVE")) / boost::filesystem::path(getenv("HOMEPATH"));
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
	BOOL p;
	IsWow64Process (GetCurrentProcess(), &p);
	return p;
}

static optional<string>
get_friendly_name (HDEVINFO device_info, SP_DEVINFO_DATA* device_info_data)
{
	wchar_t buffer[MAX_PATH];
	ZeroMemory (&buffer, sizeof(buffer));
	bool r = SetupDiGetDeviceRegistryPropertyW (
			device_info, device_info_data, SPDRP_FRIENDLYNAME, 0, reinterpret_cast<PBYTE>(buffer), sizeof(buffer), 0
			);
	if (!r) {
		return optional<string>();
	}
	return wchar_to_utf8 (buffer);
}

static const GUID GUID_DEVICE_INTERFACE_DISK = {
	0x53F56307L, 0xB6BF, 0x11D0, { 0x94, 0xF2, 0x00, 0xA0, 0xC9, 0x1E, 0xFB, 0x8B }
};

static optional<int>
get_device_number (HDEVINFO device_info, SP_DEVINFO_DATA* device_info_data)
{
	/* Find the Windows path to the device */

	SP_DEVICE_INTERFACE_DATA device_interface_data;
	device_interface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

	BOOL r = SetupDiEnumDeviceInterfaces (device_info, device_info_data, &GUID_DEVICE_INTERFACE_DISK, 0, &device_interface_data);
	if (!r) {
		LOG_DISK("SetupDiEnumDeviceInterfaces failed (%1)", GetLastError());
		return optional<int>();
	}

	/* Find out how much space we need for our SP_DEVICE_INTERFACE_DETAIL_DATA_W */
	DWORD size;
	r = SetupDiGetDeviceInterfaceDetailW(device_info, &device_interface_data, 0, 0, &size, 0);
	PSP_DEVICE_INTERFACE_DETAIL_DATA_W device_detail_data = static_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W> (malloc(size));
	if (!device_detail_data) {
		LOG_DISK_NC("malloc failed");
		return optional<int>();
	}

	device_detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

	/* And get the path */
	r = SetupDiGetDeviceInterfaceDetailW (device_info, &device_interface_data, device_detail_data, size, &size, 0);
	if (!r) {
		LOG_DISK_NC("SetupDiGetDeviceInterfaceDetailW failed");
		free (device_detail_data);
		return optional<int>();
	}

	/* Open it.  We would not be allowed GENERIC_READ access here but specifying 0 for
	   dwDesiredAccess allows us to query some metadata.
	*/
	HANDLE device = CreateFileW (
			device_detail_data->DevicePath, 0,
			FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
			OPEN_EXISTING, 0, 0
			);

	free (device_detail_data);

	if (device == INVALID_HANDLE_VALUE) {
		LOG_DISK("CreateFileW failed with %1", GetLastError());
		return optional<int>();
	}

	/* Get the device number */
	STORAGE_DEVICE_NUMBER device_number;
	r = DeviceIoControl (
			device, IOCTL_STORAGE_GET_DEVICE_NUMBER, 0, 0,
			&device_number, sizeof(device_number), &size, 0
			);

	CloseHandle (device);

	if (!r) {
		return optional<int>();
	}

	return device_number.DeviceNumber;
}

/** Take a volume path (with a trailing \) and add any disk numbers related to that volume
 *  to @ref disks.
 */
static void
add_volume_disk_number (wchar_t* volume, vector<int>& disks)
{
	/* Strip trailing \ */
	size_t const len = wcslen (volume);
	DCPOMATIC_ASSERT (len > 0);
	volume[len - 1] = L'\0';

	HANDLE handle = CreateFileW (
			volume, 0,
			FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
			OPEN_EXISTING, 0, 0
			);

	DCPOMATIC_ASSERT (handle != INVALID_HANDLE_VALUE);

	VOLUME_DISK_EXTENTS extents;
	DWORD size;
	BOOL r = DeviceIoControl (handle, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, 0, 0, &extents, sizeof(extents), &size, 0);
	CloseHandle (handle);
	if (!r) {
		return;
	}
	DCPOMATIC_ASSERT (extents.NumberOfDiskExtents == 1);
	return disks.push_back (extents.Extents[0].DiskNumber);
}

/* Return a list of disk numbers that contain volumes; i.e. a list of disk numbers that should
 * not be offered as targets to write to as they are "mounted" (whatever that means on Windows).
 */
vector<int>
disk_numbers_with_volumes ()
{
	vector<int> disks;

	wchar_t volume_name[512];
	HANDLE volume = FindFirstVolumeW (volume_name, sizeof(volume_name) / sizeof(wchar_t));
	if (volume == INVALID_HANDLE_VALUE) {
		return disks;
	}

	add_volume_disk_number (volume_name, disks);
	while (true) {
		if (!FindNextVolumeW(volume, volume_name, sizeof(volume_name) / sizeof(wchar_t))) {
			break;
		}
		add_volume_disk_number (volume_name, disks);
	}
	FindVolumeClose (volume);

	return disks;
}

vector<Drive>
get_drives ()
{
	vector<Drive> drives;

	vector<int> disks_to_ignore = disk_numbers_with_volumes ();

	/* Get a `device information set' containing information about all disks */
	HDEVINFO device_info = SetupDiGetClassDevsA (&GUID_DEVICE_INTERFACE_DISK, 0, 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	if (device_info == INVALID_HANDLE_VALUE) {
		LOG_DISK_NC ("SetupDiClassDevsA failed");
		return drives;
	}

	int i = 0;
	while (true) {
		/* Find out about the next disk */
		SP_DEVINFO_DATA device_info_data;
		device_info_data.cbSize = sizeof(SP_DEVINFO_DATA);
		if (!SetupDiEnumDeviceInfo(device_info, i, &device_info_data)) {
			DWORD e = GetLastError();
			if (e != ERROR_NO_MORE_ITEMS) {
				LOG_DISK ("SetupDiEnumDeviceInfo failed (%1)", GetLastError());
			}
			break;
		}
		++i;

		optional<string> const friendly_name = get_friendly_name (device_info, &device_info_data);
		optional<int> device_number = get_device_number (device_info, &device_info_data);
		if (!device_number) {
			continue;
		}

		string const physical_drive = String::compose("\\\\.\\PHYSICALDRIVE%1", *device_number);

		HANDLE device = CreateFileA (
				physical_drive.c_str(), 0,
				FILE_SHARE_READ | FILE_SHARE_WRITE, 0,
				OPEN_EXISTING, 0, 0
				);

		if (device == INVALID_HANDLE_VALUE) {
			LOG_DISK_NC("Could not open PHYSICALDRIVE");
			continue;
		}

		DISK_GEOMETRY geom;
		DWORD returned;
		BOOL r = DeviceIoControl (
				device, IOCTL_DISK_GET_DRIVE_GEOMETRY, 0, 0,
				&geom, sizeof(geom), &returned, 0
				);

		if (r && find(disks_to_ignore.begin(), disks_to_ignore.end(), *device_number) == disks_to_ignore.end()) {
			uint64_t const disk_size = geom.Cylinders.QuadPart * geom.TracksPerCylinder * geom.SectorsPerTrack * geom.BytesPerSector;
			drives.push_back (Drive(physical_drive, disk_size, false, friendly_name, optional<string>()));
		}

		CloseHandle (device);
	}

	return drives;
}

boost::filesystem::path
config_path ()
{
	boost::filesystem::path p;
	p /= g_get_user_config_dir ();
	p /= "dcpomatic2";
	return p;
}
