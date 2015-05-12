/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

/** @file src/lib/util.cc
 *  @brief Some utility functions and classes.
 */

#include "util.h"
#include "exceptions.h"
#include "dcp_content_type.h"
#include "filter.h"
#include "cinema_sound_processor.h"
#include "config.h"
#include "ratio.h"
#include "job.h"
#include "cross.h"
#include "video_content.h"
#include "rect.h"
#include "md5_digester.h"
#include "audio_processor.h"
#include "safe_stringstream.h"
#include <dcp/util.h>
#include <dcp/signer.h>
#include <glib.h>
#include <pangomm/init.h>
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#ifdef DCPOMATIC_WINDOWS
#include <boost/locale.hpp>
#include <dbghelp.h>
#endif
#include <signal.h>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <climits>
#include <stdexcept>
#ifdef DCPOMATIC_POSIX
#include <execinfo.h>
#include <cxxabi.h>
#endif

#include "i18n.h"

using std::string;
using std::setfill;
using std::ostream;
using std::endl;
using std::vector;
using std::min;
using std::max;
using std::map;
using std::list;
using std::multimap;
using std::istream;
using std::pair;
using std::cout;
using std::bad_alloc;
using std::set_terminate;
using boost::shared_ptr;
using boost::thread;
using boost::optional;
using dcp::Size;

/** Path to our executable, required by the stacktrace stuff and filled
 *  in during App::onInit().
 */
string program_name;
static boost::thread::id ui_thread;
static boost::filesystem::path backtrace_file;

/** Convert some number of seconds to a string representation
 *  in hours, minutes and seconds.
 *
 *  @param s Seconds.
 *  @return String of the form H:M:S (where H is hours, M
 *  is minutes and S is seconds).
 */
string
seconds_to_hms (int s)
{
	int m = s / 60;
	s -= (m * 60);
	int h = m / 60;
	m -= (h * 60);

	SafeStringStream hms;
	hms << h << N_(":");
	hms.width (2);
	hms << setfill ('0') << m << N_(":");
	hms.width (2);
	hms << setfill ('0') << s;

	return hms.str ();
}

/** @param s Number of seconds.
 *  @return String containing an approximate description of s (e.g. "about 2 hours")
 */
string
seconds_to_approximate_hms (int s)
{
	int m = s / 60;
	s -= (m * 60);
	int h = m / 60;
	m -= (h * 60);

	SafeStringStream ap;

	bool const hours = h > 0;
	bool const minutes = h < 10 && m > 0;
	bool const seconds = m < 10 && s > 0;

	if (hours) {
		if (m > 30 && !minutes) {
			/// TRANSLATORS: h here is an abbreviation for hours
			ap << (h + 1) << _("h");
		} else {
			/// TRANSLATORS: h here is an abbreviation for hours
			ap << h << _("h");
		}

		if (minutes | seconds) {
			ap << N_(" ");
		}
	}

	if (minutes) {
		/* Minutes */
		if (s > 30 && !seconds) {
			/// TRANSLATORS: m here is an abbreviation for minutes
			ap << (m + 1) << _("m");
		} else {
			/// TRANSLATORS: m here is an abbreviation for minutes
			ap << m << _("m");
		}

		if (seconds) {
			ap << N_(" ");
		}
	}

	if (seconds) {
		/* Seconds */
		/// TRANSLATORS: s here is an abbreviation for seconds
		ap << s << _("s");
	}

	return ap.str ();
}

double
seconds (struct timeval t)
{
	return t.tv_sec + (double (t.tv_usec) / 1e6);
}

#ifdef DCPOMATIC_WINDOWS

/** Resolve symbol name and source location given the path to the executable */
int
addr2line (void const * const addr)
{
	char addr2line_cmd[512] = { 0 };
	sprintf (addr2line_cmd, "addr2line -f -p -e %.256s %p > %s", program_name.c_str(), addr, backtrace_file.string().c_str()); 
	return system(addr2line_cmd);
}

/** This is called when C signals occur on Windows (e.g. SIGSEGV)
 *  (NOT C++ exceptions!).  We write a backtrace to backtrace_file by dark means.
 *  Adapted from code here: http://spin.atomicobject.com/2013/01/13/exceptions-stack-traces-c/
 */
LONG WINAPI
exception_handler(struct _EXCEPTION_POINTERS * info)
{
	FILE* f = fopen_boost (backtrace_file, "w");
	fprintf (f, "C-style exception %d\n", info->ExceptionRecord->ExceptionCode);
	fclose(f);
	
	if (info->ExceptionRecord->ExceptionCode != EXCEPTION_STACK_OVERFLOW) {
		CONTEXT* context = info->ContextRecord;
		SymInitialize (GetCurrentProcess (), 0, true);
		
		STACKFRAME frame = { 0 };
		
		/* setup initial stack frame */
#if _WIN64
		frame.AddrPC.Offset    = context->Rip;
		frame.AddrStack.Offset = context->Rsp;
		frame.AddrFrame.Offset = context->Rbp;
#else  
		frame.AddrPC.Offset    = context->Eip;
		frame.AddrStack.Offset = context->Esp;
		frame.AddrFrame.Offset = context->Ebp;
#endif
		frame.AddrPC.Mode      = AddrModeFlat;
		frame.AddrStack.Mode   = AddrModeFlat;
		frame.AddrFrame.Mode   = AddrModeFlat;
		
		while (
			StackWalk (
				IMAGE_FILE_MACHINE_I386,
				GetCurrentProcess (),
				GetCurrentThread (),
				&frame,
				context,
				0,
				SymFunctionTableAccess,
				SymGetModuleBase,
				0
				)
			) {
			addr2line((void *) frame.AddrPC.Offset);
		}
	} else {
#ifdef _WIN64          
		addr2line ((void *) info->ContextRecord->Rip);
#else          
		addr2line ((void *) info->ContextRecord->Eip);
#endif         
	}
	
	return EXCEPTION_CONTINUE_SEARCH;
}
#endif

void
set_backtrace_file (boost::filesystem::path p)
{
	backtrace_file = p;
}

/** This is called when there is an unhandled exception.  Any
 *  backtrace in this function is useless on Windows as the stack has
 *  already been unwound from the throw; we have the gdb wrap hack to
 *  cope with that.
 */
void
terminate ()
{
	try {
		static bool tried_throw = false;
		// try once to re-throw currently active exception
		if (!tried_throw) {
			tried_throw = true;
			throw;
		}
	}
	catch (const std::exception &e) {
		std::cerr << __FUNCTION__ << " caught unhandled exception. what(): "
			  << e.what() << std::endl;
	}
	catch (...) {
		std::cerr << __FUNCTION__ << " caught unknown/unhandled exception." 
			  << std::endl;
	}

	abort();
}

/** Call the required functions to set up DCP-o-matic's static arrays, etc.
 *  Must be called from the UI thread, if there is one.
 */
void
dcpomatic_setup ()
{
#ifdef DCPOMATIC_WINDOWS
	boost::filesystem::path p = g_get_user_config_dir ();
	p /= "backtrace.txt";
	set_backtrace_file (p);
	SetUnhandledExceptionFilter(exception_handler);

	/* Dark voodoo which, I think, gets boost::filesystem::path to
	   correctly convert UTF-8 strings to paths, and also paths
	   back to UTF-8 strings (on path::string()).

	   After this, constructing boost::filesystem::paths from strings
	   converts from UTF-8 to UTF-16 inside the path.  Then
	   path::string().c_str() gives UTF-8 and
	   path::c_str()          gives UTF-16.

	   This is all Windows-only.  AFAICT Linux/OS X use UTF-8 everywhere,
	   so things are much simpler.
	*/
	std::locale::global (boost::locale::generator().generate (""));
	boost::filesystem::path::imbue (std::locale ());
#endif	
	
	avfilter_register_all ();

#ifdef DCPOMATIC_OSX
	/* Add our lib directory to the libltdl search path so that
	   xmlsec can find xmlsec1-openssl.
	*/
	boost::filesystem::path lib = app_contents ();
	lib /= "lib";
	setenv ("LTDL_LIBRARY_PATH", lib.c_str (), 1);
#endif

	set_terminate (terminate);

	Pango::init ();
	dcp::init ();
	
	Ratio::setup_ratios ();
	VideoContentScale::setup_scales ();
	DCPContentType::setup_dcp_content_types ();
	Filter::setup_filters ();
	CinemaSoundProcessor::setup_cinema_sound_processors ();
	AudioProcessor::setup_audio_processors ();

	ui_thread = boost::this_thread::get_id ();
}

#ifdef DCPOMATIC_WINDOWS
boost::filesystem::path
mo_path ()
{
	wchar_t buffer[512];
	GetModuleFileName (0, buffer, 512 * sizeof(wchar_t));
	boost::filesystem::path p (buffer);
	p = p.parent_path ();
	p = p.parent_path ();
	p /= "locale";
	return p;
}
#endif

#ifdef DCPOMATIC_OSX
boost::filesystem::path
mo_path ()
{
	return "DCP-o-matic 2.app/Contents/Resources";
}
#endif

void
dcpomatic_setup_gettext_i18n (string lang)
{
#ifdef DCPOMATIC_LINUX
	lang += ".UTF8";
#endif

	if (!lang.empty ()) {
		/* Override our environment language.  Note that the caller must not
		   free the string passed into putenv().
		*/
		string s = String::compose ("LANGUAGE=%1", lang);
		putenv (strdup (s.c_str ()));
		s = String::compose ("LANG=%1", lang);
		putenv (strdup (s.c_str ()));
		s = String::compose ("LC_ALL=%1", lang);
		putenv (strdup (s.c_str ()));
	}

	setlocale (LC_ALL, "");
	textdomain ("libdcpomatic");

#if defined(DCPOMATIC_WINDOWS) || defined(DCPOMATIC_OSX)
	bindtextdomain ("libdcpomatic", mo_path().string().c_str());
	bind_textdomain_codeset ("libdcpomatic", "UTF8");
#endif	

#ifdef DCPOMATIC_LINUX
	bindtextdomain ("libdcpomatic", LINUX_LOCALE_PREFIX);
#endif
}

/** Compute a digest of the first and last `size' bytes of a set of files. */
string
md5_digest_head_tail (vector<boost::filesystem::path> files, boost::uintmax_t size)
{
	boost::scoped_array<char> buffer (new char[size]);
	MD5Digester digester;

	/* Head */
	boost::uintmax_t to_do = size;
	char* p = buffer.get ();
	int i = 0;
	while (i < int64_t (files.size()) && to_do > 0) {
		FILE* f = fopen_boost (files[i], "rb");
		if (!f) {
			throw OpenFileError (files[i].string());
		}

		boost::uintmax_t this_time = min (to_do, boost::filesystem::file_size (files[i]));
		fread (p, 1, this_time, f);
		p += this_time;
 		to_do -= this_time;
		fclose (f);

		++i;
	}
	digester.add (buffer.get(), size - to_do);

	/* Tail */
	to_do = size;
	p = buffer.get ();
	i = files.size() - 1;
	while (i >= 0 && to_do > 0) {
		FILE* f = fopen_boost (files[i], "rb");
		if (!f) {
			throw OpenFileError (files[i].string());
		}

		boost::uintmax_t this_time = min (to_do, boost::filesystem::file_size (files[i]));
		dcpomatic_fseek (f, -this_time, SEEK_END);
		fread (p, 1, this_time, f);
		p += this_time;
		to_do -= this_time;
		fclose (f);

		--i;
	}		
	digester.add (buffer.get(), size - to_do);

	return digester.get ();
}

/** @param An arbitrary audio frame rate.
 *  @return The appropriate DCP-approved frame rate (48kHz or 96kHz).
 */
int
dcp_audio_frame_rate (int fs)
{
	if (fs <= 48000) {
		return 48000;
	}

	return 96000;
}

/** Round a number up to the nearest multiple of another number.
 *  @param c Index.
 *  @param s Array of numbers to round, indexed by c.
 *  @param t Multiple to round to.
 *  @return Rounded number.
 */
int
stride_round_up (int c, int const * stride, int t)
{
	int const a = stride[c] + (t - 1);
	return a - (a % t);
}

/** @param n A number.
 *  @param r Rounding `boundary' (must be a power of 2)
 *  @return n rounded to the nearest r
 */
int
round_to (float n, int r)
{
	DCPOMATIC_ASSERT (r == 1 || r == 2 || r == 4);
	return int (n + float(r) / 2) &~ (r - 1);
}

/** Trip an assert if the caller is not in the UI thread */
void
ensure_ui_thread ()
{
	DCPOMATIC_ASSERT (boost::this_thread::get_id() == ui_thread);
}

string
audio_channel_name (int c)
{
	DCPOMATIC_ASSERT (MAX_DCP_AUDIO_CHANNELS == 12);

	/// TRANSLATORS: these are the names of audio channels; Lfe (sub) is the low-frequency
	/// enhancement channel (sub-woofer).  HI is the hearing-impaired audio track and
	/// VI is the visually-impaired audio track (audio describe).
	string const channels[] = {
		_("Left"),
		_("Right"),
		_("Centre"),
		_("Lfe (sub)"),
		_("Left surround"),
		_("Right surround"),
		_("Hearing impaired"),
		_("Visually impaired"),
		_("Left centre"),
		_("Right centre"),
		_("Left rear surround"),
		_("Right rear surround"),
	};

	return channels[c];
}

bool
valid_image_file (boost::filesystem::path f)
{
	if (boost::starts_with (f.leaf().string(), "._")) {
		return false;
	}
		
	string ext = f.extension().string();
	transform (ext.begin(), ext.end(), ext.begin(), ::tolower);
	return (
		ext == ".tif" || ext == ".tiff" || ext == ".jpg" || ext == ".jpeg" ||
		ext == ".png" || ext == ".bmp" || ext == ".tga" || ext == ".dpx" ||
		ext == ".j2c" || ext == ".j2k"
		);
}

bool
valid_j2k_file (boost::filesystem::path f)
{
	string ext = f.extension().string();
	transform (ext.begin(), ext.end(), ext.begin(), ::tolower);
	return (ext == ".j2k" || ext == ".j2c");
}

string
tidy_for_filename (string f)
{
	string t;
	for (size_t i = 0; i < f.length(); ++i) {
		if (isalnum (f[i]) || f[i] == '_' || f[i] == '-') {
			t += f[i];
		} else {
			t += '_';
		}
	}

	return t;
}

dcp::Size
fit_ratio_within (float ratio, dcp::Size full_frame)
{
	if (ratio < full_frame.ratio ()) {
		return dcp::Size (rint (full_frame.height * ratio), full_frame.height);
	}
	
	return dcp::Size (full_frame.width, rint (full_frame.width / ratio));
}

void *
wrapped_av_malloc (size_t s)
{
	void* p = av_malloc (s);
	if (!p) {
		throw bad_alloc ();
	}
	return p;
}

FFmpegSubtitlePeriod
subtitle_period (AVSubtitle const & sub)
{
	ContentTime const packet_time = ContentTime::from_seconds (static_cast<double> (sub.pts) / AV_TIME_BASE);

	if (sub.end_display_time == static_cast<uint32_t> (-1)) {
		/* End time is not known */
		return FFmpegSubtitlePeriod (packet_time + ContentTime::from_seconds (sub.start_display_time / 1e3));
	}
	
	return FFmpegSubtitlePeriod (
		packet_time + ContentTime::from_seconds (sub.start_display_time / 1e3),
		packet_time + ContentTime::from_seconds (sub.end_display_time / 1e3)
		);
}

map<string, string>
split_get_request (string url)
{
	enum {
		AWAITING_QUESTION_MARK,
		KEY,
		VALUE
	} state = AWAITING_QUESTION_MARK;
	
	map<string, string> r;
	string k;
	string v;
	for (size_t i = 0; i < url.length(); ++i) {
		switch (state) {
		case AWAITING_QUESTION_MARK:
			if (url[i] == '?') {
				state = KEY;
			}
			break;
		case KEY:
			if (url[i] == '=') {
				v.clear ();
				state = VALUE;
			} else {
				k += url[i];
			}
			break;
		case VALUE:
			if (url[i] == '&') {
				r.insert (make_pair (k, v));
				k.clear ();
				state = KEY;
			} else {
				v += url[i];
			}
			break;
		}
	}

	if (state == VALUE) {
		r.insert (make_pair (k, v));
	}

	return r;
}

long
frame_info_position (int frame, Eyes eyes)
{
	static int const info_size = 48;
	
	switch (eyes) {
	case EYES_BOTH:
		return frame * info_size;
	case EYES_LEFT:
		return frame * info_size * 2;
	case EYES_RIGHT:
		return frame * info_size * 2 + info_size;
	default:
		DCPOMATIC_ASSERT (false);
	}

	DCPOMATIC_ASSERT (false);
}

dcp::FrameInfo
read_frame_info (FILE* file, int frame, Eyes eyes)
{
	dcp::FrameInfo info;
	dcpomatic_fseek (file, frame_info_position (frame, eyes), SEEK_SET);
	fread (&info.offset, sizeof (info.offset), 1, file);
	fread (&info.size, sizeof (info.size), 1, file);
	
	char hash_buffer[33];
	fread (hash_buffer, 1, 32, file);
	hash_buffer[32] = '\0';
	info.hash = hash_buffer;

	return info;
}

void
write_frame_info (FILE* file, int frame, Eyes eyes, dcp::FrameInfo info)
{
	dcpomatic_fseek (file, frame_info_position (frame, eyes), SEEK_SET);
	fwrite (&info.offset, sizeof (info.offset), 1, file);
	fwrite (&info.size, sizeof (info.size), 1, file);
	fwrite (info.hash.c_str(), 1, info.hash.size(), file);
}
