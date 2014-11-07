/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>
    Copyright (C) 2000-2007 Paul Davis

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

#include <iomanip>
#include <iostream>
#include <fstream>
#include <climits>
#include <stdexcept>
#ifdef DCPOMATIC_POSIX
#include <execinfo.h>
#include <cxxabi.h>
#endif
#include <libssh/libssh.h>
#include <signal.h>
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#ifdef DCPOMATIC_WINDOWS
#include <boost/locale.hpp>
#endif
#include <glib.h>
#include <openjpeg.h>
#include <pangomm/init.h>
#ifdef DCPOMATIC_IMAGE_MAGICK
#include <magick/MagickCore.h>
#else
#include <magick/common.h>
#include <magick/magick_config.h>
#endif
#include <magick/version.h>
#include <dcp/version.h>
#include <dcp/util.h>
#include <dcp/signer.h>
#include <dcp/raw_convert.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavfilter/avfiltergraph.h>
#include <libavutil/pixfmt.h>
}
#include "util.h"
#include "exceptions.h"
#include "scaler.h"
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
#ifdef DCPOMATIC_WINDOWS
#include "stack.hpp"
#endif

#include "i18n.h"

using std::string;
using std::setfill;
using std::ostream;
using std::endl;
using std::vector;
using std::hex;
using std::setw;
using std::ios;
using std::min;
using std::max;
using std::list;
using std::multimap;
using std::map;
using std::istream;
using std::numeric_limits;
using std::pair;
using std::cout;
using std::bad_alloc;
using std::streampos;
using std::set_terminate;
using boost::shared_ptr;
using boost::thread;
using boost::optional;
using dcp::Size;
using dcp::raw_convert;

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
	hms << std::setfill ('0') << m << N_(":");
	hms.width (2);
	hms << std::setfill ('0') << s;

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
			/* TRANSLATORS: h here is an abbreviation for hours */
			ap << (h + 1) << _("h");
		} else {
			/* TRANSLATORS: h here is an abbreviation for hours */
			ap << h << _("h");
		}

		if (minutes | seconds) {
			ap << N_(" ");
		}
	}

	if (minutes) {
		/* Minutes */
		if (s > 30 && !seconds) {
			/* TRANSLATORS: m here is an abbreviation for minutes */
			ap << (m + 1) << _("m");
		} else {
			/* TRANSLATORS: m here is an abbreviation for minutes */
			ap << m << _("m");
		}

		if (seconds) {
			ap << N_(" ");
		}
	}

	if (seconds) {
		/* Seconds */
		/* TRANSLATORS: s here is an abbreviation for seconds */
		ap << s << _("s");
	}

	return ap.str ();
}

#ifdef DCPOMATIC_POSIX
/** @param l Mangled C++ identifier.
 *  @return Demangled version.
 */
static string
demangle (string l)
{
	string::size_type const b = l.find_first_of (N_("("));
	if (b == string::npos) {
		return l;
	}

	string::size_type const p = l.find_last_of (N_("+"));
	if (p == string::npos) {
		return l;
	}

	if ((p - b) <= 1) {
		return l;
	}
	
	string const fn = l.substr (b + 1, p - b - 1);

	int status;
	try {
		
		char* realname = abi::__cxa_demangle (fn.c_str(), 0, 0, &status);
		string d (realname);
		free (realname);
		return d;
		
	} catch (std::exception) {
		
	}
	
	return l;
}

/** Write a stacktrace to an ostream.
 *  @param out Stream to write to.
 *  @param levels Number of levels to go up the call stack.
 */
void
stacktrace (ostream& out, int levels)
{
	void *array[200];
	size_t size = backtrace (array, 200);
	char** strings = backtrace_symbols (array, size);
     
	if (strings) {
		for (size_t i = 0; i < size && (levels == 0 || i < size_t(levels)); i++) {
			out << N_("  ") << demangle (strings[i]) << "\n";
		}
		
		free (strings);
	}
}
#endif

/** @param v Version as used by FFmpeg.
 *  @return A string representation of v.
 */
static string
ffmpeg_version_to_string (int v)
{
	SafeStringStream s;
	s << ((v & 0xff0000) >> 16) << N_(".") << ((v & 0xff00) >> 8) << N_(".") << (v & 0xff);
	return s.str ();
}

double
seconds (struct timeval t)
{
	return t.tv_sec + (double (t.tv_usec) / 1e6);
}

#ifdef DCPOMATIC_WINDOWS
LONG WINAPI exception_handler(struct _EXCEPTION_POINTERS *)
{
	dbg::stack s;
	FILE* f = fopen_boost (backtrace_file, "w");
	fprintf (f, "Exception thrown:");
	for (dbg::stack::const_iterator i = s.begin(); i != s.end(); ++i) {
		fprintf (f, "%p %s %d %s\n", i->instruction, i->function.c_str(), i->line, i->module.c_str());
	}
	fclose (f);
	return EXCEPTION_CONTINUE_SEARCH;
}
#endif

/* From http://stackoverflow.com/questions/2443135/how-do-i-find-where-an-exception-was-thrown-in-c */
void
terminate ()
{
	static bool tried_throw = false;

	try {
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

#ifdef DCPOMATIC_POSIX
	stacktrace (cout, 50);
#endif
	abort();
}

/** Call the required functions to set up DCP-o-matic's static arrays, etc.
 *  Must be called from the UI thread, if there is one.
 */
void
dcpomatic_setup ()
{
#ifdef DCPOMATIC_WINDOWS
	backtrace_file /= g_get_user_config_dir ();
	backtrace_file /= "backtrace.txt";
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
	Scaler::setup_scalers ();
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
	bindtextdomain ("libdcpomatic", POSIX_LOCALE_PREFIX);
#endif
}

/** @param s A string.
 *  @return Parts of the string split at spaces, except when a space is within quotation marks.
 */
vector<string>
split_at_spaces_considering_quotes (string s)
{
	vector<string> out;
	bool in_quotes = false;
	string c;
	for (string::size_type i = 0; i < s.length(); ++i) {
		if (s[i] == ' ' && !in_quotes) {
			out.push_back (c);
			c = N_("");
		} else if (s[i] == '"') {
			in_quotes = !in_quotes;
		} else {
			c += s[i];
		}
	}

	out.push_back (c);
	return out;
}

/** @param job Optional job for which to report progress */
string
md5_digest (vector<boost::filesystem::path> files, shared_ptr<Job> job)
{
	boost::uintmax_t const buffer_size = 64 * 1024;
	char buffer[buffer_size];

	MD5Digester digester;

	vector<int64_t> sizes;
	for (size_t i = 0; i < files.size(); ++i) {
		sizes.push_back (boost::filesystem::file_size (files[i]));
	}

	for (size_t i = 0; i < files.size(); ++i) {
		FILE* f = fopen_boost (files[i], "rb");
		if (!f) {
			throw OpenFileError (files[i].string());
		}

		boost::uintmax_t const bytes = boost::filesystem::file_size (files[i]);
		boost::uintmax_t remaining = bytes;

		while (remaining > 0) {
			int const t = min (remaining, buffer_size);
			int const r = fread (buffer, 1, t, f);
			if (r != t) {
				throw ReadFileError (files[i], errno);
			}
			digester.add (buffer, t);
			remaining -= t;

			if (job) {
				job->set_progress ((float (i) + 1 - float(remaining) / bytes) / files.size ());
			}
		}

		fclose (f);
	}

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

Socket::Socket (int timeout)
	: _deadline (_io_service)
	, _socket (_io_service)
	, _acceptor (0)
	, _timeout (timeout)
{
	_deadline.expires_at (boost::posix_time::pos_infin);
	check ();
}

Socket::~Socket ()
{
	delete _acceptor;
}

void
Socket::check ()
{
	if (_deadline.expires_at() <= boost::asio::deadline_timer::traits_type::now ()) {
		if (_acceptor) {
			_acceptor->cancel ();
		} else {
			_socket.close ();
		}
		_deadline.expires_at (boost::posix_time::pos_infin);
	}

	_deadline.async_wait (boost::bind (&Socket::check, this));
}

/** Blocking connect.
 *  @param endpoint End-point to connect to.
 */
void
Socket::connect (boost::asio::ip::tcp::endpoint endpoint)
{
	_deadline.expires_from_now (boost::posix_time::seconds (_timeout));
	boost::system::error_code ec = boost::asio::error::would_block;
	_socket.async_connect (endpoint, boost::lambda::var(ec) = boost::lambda::_1);
	do {
		_io_service.run_one();
	} while (ec == boost::asio::error::would_block);

	if (ec) {
		throw NetworkError (String::compose (_("error during async_connect (%1)"), ec.value ()));
	}

	if (!_socket.is_open ()) {
		throw NetworkError (_("connect timed out"));
	}
}

void
Socket::accept (int port)
{
	_acceptor = new boost::asio::ip::tcp::acceptor (_io_service, boost::asio::ip::tcp::endpoint (boost::asio::ip::tcp::v4(), port));
	
	_deadline.expires_from_now (boost::posix_time::seconds (_timeout));
	boost::system::error_code ec = boost::asio::error::would_block;
	_acceptor->async_accept (_socket, boost::lambda::var(ec) = boost::lambda::_1);
	do {
		_io_service.run_one ();
	} while (ec == boost::asio::error::would_block);

	delete _acceptor;
	_acceptor = 0;
	
	if (ec) {
		throw NetworkError (String::compose (_("error during async_accept (%1)"), ec.value ()));
	}
}

/** Blocking write.
 *  @param data Buffer to write.
 *  @param size Number of bytes to write.
 */
void
Socket::write (uint8_t const * data, int size)
{
	_deadline.expires_from_now (boost::posix_time::seconds (_timeout));
	boost::system::error_code ec = boost::asio::error::would_block;

	boost::asio::async_write (_socket, boost::asio::buffer (data, size), boost::lambda::var(ec) = boost::lambda::_1);
	
	do {
		_io_service.run_one ();
	} while (ec == boost::asio::error::would_block);

	if (ec) {
		throw NetworkError (String::compose (_("error during async_write (%1)"), ec.value ()));
	}
}

void
Socket::write (uint32_t v)
{
	v = htonl (v);
	write (reinterpret_cast<uint8_t*> (&v), 4);
}

/** Blocking read.
 *  @param data Buffer to read to.
 *  @param size Number of bytes to read.
 */
void
Socket::read (uint8_t* data, int size)
{
	_deadline.expires_from_now (boost::posix_time::seconds (_timeout));
	boost::system::error_code ec = boost::asio::error::would_block;

	boost::asio::async_read (_socket, boost::asio::buffer (data, size), boost::lambda::var(ec) = boost::lambda::_1);

	do {
		_io_service.run_one ();
	} while (ec == boost::asio::error::would_block);
	
	if (ec) {
		throw NetworkError (String::compose (_("error during async_read (%1)"), ec.value ()));
	}
}

uint32_t
Socket::read_uint32 ()
{
	uint32_t v;
	read (reinterpret_cast<uint8_t *> (&v), 4);
	return ntohl (v);
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
	assert (r == 1 || r == 2 || r == 4);
	return int (n + float(r) / 2) &~ (r - 1);
}

/** Read a sequence of key / value pairs from a text stream;
 *  the keys are the first words on the line, and the values are
 *  the remainder of the line following the key.  Lines beginning
 *  with # are ignored.
 *  @param s Stream to read.
 *  @return key/value pairs.
 */
multimap<string, string>
read_key_value (istream &s) 
{
	multimap<string, string> kv;
	
	string line;
	while (getline (s, line)) {
		if (line.empty ()) {
			continue;
		}

		if (line[0] == '#') {
			continue;
		}

		if (line[line.size() - 1] == '\r') {
			line = line.substr (0, line.size() - 1);
		}

		size_t const s = line.find (' ');
		if (s == string::npos) {
			continue;
		}

		kv.insert (make_pair (line.substr (0, s), line.substr (s + 1)));
	}

	return kv;
}

string
get_required_string (multimap<string, string> const & kv, string k)
{
	if (kv.count (k) > 1) {
		throw StringError (N_("unexpected multiple keys in key-value set"));
	}

	multimap<string, string>::const_iterator i = kv.find (k);
	
	if (i == kv.end ()) {
		throw StringError (String::compose (_("missing key %1 in key-value set"), k));
	}

	return i->second;
}

int
get_required_int (multimap<string, string> const & kv, string k)
{
	string const v = get_required_string (kv, k);
	return raw_convert<int> (v);
}

float
get_required_float (multimap<string, string> const & kv, string k)
{
	string const v = get_required_string (kv, k);
	return raw_convert<float> (v);
}

string
get_optional_string (multimap<string, string> const & kv, string k)
{
	if (kv.count (k) > 1) {
		throw StringError (N_("unexpected multiple keys in key-value set"));
	}

	multimap<string, string>::const_iterator i = kv.find (k);
	if (i == kv.end ()) {
		return N_("");
	}

	return i->second;
}

int
get_optional_int (multimap<string, string> const & kv, string k)
{
	if (kv.count (k) > 1) {
		throw StringError (N_("unexpected multiple keys in key-value set"));
	}

	multimap<string, string>::const_iterator i = kv.find (k);
	if (i == kv.end ()) {
		return 0;
	}

	return raw_convert<int> (i->second);
}

/** Trip an assert if the caller is not in the UI thread */
void
ensure_ui_thread ()
{
	assert (boost::this_thread::get_id() == ui_thread);
}

string
audio_channel_name (int c)
{
	assert (MAX_DCP_AUDIO_CHANNELS == 12);

	/* TRANSLATORS: these are the names of audio channels; Lfe (sub) is the low-frequency
	   enhancement channel (sub-woofer).  HI is the hearing-impaired audio track and
	   VI is the visually-impaired audio track (audio describe).
	*/
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
	string ext = f.extension().string();
	transform (ext.begin(), ext.end(), ext.begin(), ::tolower);
	return (ext == ".tif" || ext == ".tiff" || ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp" || ext == ".tga" || ext == ".dpx");
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

dcp::Size
fit_ratio_within (float ratio, dcp::Size full_frame, int round)
{
	if (ratio < full_frame.ratio ()) {
		return dcp::Size (round_to (full_frame.height * ratio, round), full_frame.height);
	}
	
	return dcp::Size (full_frame.width, round_to (full_frame.width / ratio, round));
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
		
string
entities_to_text (string e)
{
	boost::algorithm::replace_all (e, "%3A", ":");
	boost::algorithm::replace_all (e, "%2F", "/");
	return e;
}

int64_t
divide_with_round (int64_t a, int64_t b)
{
	if (a % b >= (b / 2)) {
		return (a + b - 1) / b;
	} else {
		return a / b;
	}
}

/** Return a user-readable string summarising the versions of our dependencies */
string
dependency_version_summary ()
{
	SafeStringStream s;
	s << N_("libopenjpeg ") << opj_version () << N_(", ")
	  << N_("libavcodec ") << ffmpeg_version_to_string (avcodec_version()) << N_(", ")
	  << N_("libavfilter ") << ffmpeg_version_to_string (avfilter_version()) << N_(", ")
	  << N_("libavformat ") << ffmpeg_version_to_string (avformat_version()) << N_(", ")
	  << N_("libavutil ") << ffmpeg_version_to_string (avutil_version()) << N_(", ")
	  << N_("libswscale ") << ffmpeg_version_to_string (swscale_version()) << N_(", ")
	  << MagickVersion << N_(", ")
	  << N_("libssh ") << ssh_version (0) << N_(", ")
	  << N_("libdcp ") << dcp::version << N_(" git ") << dcp::git_commit;

	return s.str ();
}

/** Construct a ScopedTemporary.  A temporary filename is decided but the file is not opened
 *  until ::open() is called.
 */
ScopedTemporary::ScopedTemporary ()
	: _open (0)
{
	_file = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path ();
}

/** Close and delete the temporary file */
ScopedTemporary::~ScopedTemporary ()
{
	close ();	
	boost::system::error_code ec;
	boost::filesystem::remove (_file, ec);
}

/** @return temporary filename */
char const *
ScopedTemporary::c_str () const
{
	return _file.string().c_str ();
}

/** Open the temporary file.
 *  @return File's FILE pointer.
 */
FILE*
ScopedTemporary::open (char const * params)
{
	_open = fopen (c_str(), params);
	return _open;
}

/** Close the file */
void
ScopedTemporary::close ()
{
	if (_open) {
		fclose (_open);
		_open = 0;
	}
}

ContentTimePeriod
subtitle_period (AVSubtitle const & sub)
{
	ContentTime const packet_time = ContentTime::from_seconds (static_cast<double> (sub.pts) / AV_TIME_BASE);

	ContentTimePeriod period (
		packet_time + ContentTime::from_seconds (sub.start_display_time / 1e3),
		packet_time + ContentTime::from_seconds (sub.end_display_time / 1e3)
		);

	return period;
}
