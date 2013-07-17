/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <fstream>
#include <boost/algorithm/string.hpp>
#include "cross.h"
#include "compose.hpp"
#include "log.h"
#ifdef DCPOMATIC_LINUX
#include <unistd.h>
#include <mntent.h>
#endif
#ifdef DCPOMATIC_WINDOWS
#include <windows.h>
#undef DATADIR
#include <shlwapi.h>
#endif
#ifdef DCPOMATIC_OSX
#include <sys/sysctl.h>
#endif

using std::pair;
using std::list;
using std::ifstream;
using std::string;
using std::make_pair;
using boost::shared_ptr;

void
dcpomatic_sleep (int s)
{
#ifdef DCPOMATIC_POSIX
	sleep (s);
#endif
#ifdef DCPOMATIC_WINDOWS
	Sleep (s * 1000);
#endif
}

/** @return A pair containing CPU model name and the number of processors */
pair<string, int>
cpu_info ()
{
	pair<string, int> info;
	info.second = 0;
	
#ifdef DCPOMATIC_LINUX
	ifstream f ("/proc/cpuinfo");
	while (f.good ()) {
		string l;
		getline (f, l);
		if (boost::algorithm::starts_with (l, "model name")) {
			string::size_type const c = l.find (':');
			if (c != string::npos) {
				info.first = l.substr (c + 2);
			}
		} else if (boost::algorithm::starts_with (l, "processor")) {
			++info.second;
		}
	}
#endif

#ifdef DCPOMATIC_OSX
	size_t N = sizeof (info.second);
	sysctlbyname ("hw.ncpu", &info.second, &N, 0, 0);
	char buffer[64];
	N = sizeof (buffer);
	if (sysctlbyname ("machdep.cpu.brand_string", buffer, &N, 0, 0) == 0) {
		info.first = buffer;
	}
#endif		

	return info;
}

void
run_ffprobe (boost::filesystem::path content, boost::filesystem::path out, shared_ptr<Log> log)
{
#ifdef DCPOMATIC_WINDOWS
	SECURITY_ATTRIBUTES security;
	security.nLength = sizeof (security);
	security.bInheritHandle = TRUE;
	security.lpSecurityDescriptor = 0;

	HANDLE child_stderr_read;
	HANDLE child_stderr_write;
	if (!CreatePipe (&child_stderr_read, &child_stderr_write, &security, 0)) {
		log->log ("ffprobe call failed (could not CreatePipe)");
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
		log->log ("ffprobe call failed (could not CreateProcess)");
		return;
	}

	FILE* o = fopen (out.string().c_str(), "w");
	if (!o) {
		log->log ("ffprobe call failed (could not create output file)");
		return;
	}

	CloseHandle (child_stderr_write);

	while (1) {
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
#else
	string ffprobe = "ffprobe \"" + content.string() + "\" 2> \"" + out.string() + "\"";
	log->log (String::compose ("Probing with %1", ffprobe));
	system (ffprobe.c_str ());
#endif	
}

list<pair<string, string> >
mount_info ()
{
	list<pair<string, string> > m;
	
#ifdef DCPOMATIC_LINUX
	FILE* f = setmntent ("/etc/mtab", "r");
	if (!f) {
		return m;
	}
	
	while (1) {
		struct mntent* mnt = getmntent (f);
		if (!mnt) {
			break;
		}

		m.push_back (make_pair (mnt->mnt_dir, mnt->mnt_type));
	}

	endmntent (f);
#endif

	return m;
}
