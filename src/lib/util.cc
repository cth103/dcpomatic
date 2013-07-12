/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>
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

#include <sstream>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <climits>
#ifdef DCPOMATIC_POSIX
#include <execinfo.h>
#include <cxxabi.h>
#endif
#include <libssh/libssh.h>
#include <signal.h>
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <glib.h>
#include <openjpeg.h>
#include <openssl/md5.h>
#include <magick/MagickCore.h>
#include <magick/version.h>
#include <libdcp/version.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavfilter/avfiltergraph.h>
#include <libpostproc/postprocess.h>
#include <libavutil/pixfmt.h>
}
#include "util.h"
#include "exceptions.h"
#include "scaler.h"
#include "dcp_content_type.h"
#include "filter.h"
#include "sound_processor.h"
#include "config.h"
#include "ratio.h"
#ifdef DCPOMATIC_WINDOWS
#include "stack.hpp"
#endif

#include "i18n.h"

using std::string;
using std::stringstream;
using std::setfill;
using std::ostream;
using std::endl;
using std::vector;
using std::hex;
using std::setw;
using std::ifstream;
using std::ios;
using std::min;
using std::max;
using std::list;
using std::multimap;
using std::istream;
using std::numeric_limits;
using std::pair;
using std::ofstream;
using boost::shared_ptr;
using boost::thread;
using boost::lexical_cast;
using boost::optional;
using libdcp::Size;

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

	stringstream hms;
	hms << h << N_(":");
	hms.width (2);
	hms << std::setfill ('0') << m << N_(":");
	hms.width (2);
	hms << std::setfill ('0') << s;

	return hms.str ();
}

string
time_to_hms (Time t)
{
	return seconds_to_hms (t / TIME_HZ);
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

	stringstream ap;
	
	if (h > 0) {
		if (m > 30) {
			ap << (h + 1) << N_(" ") << _("hours");
		} else {
			if (h == 1) {
				ap << N_("1 ") << _("hour");
			} else {
				ap << h << N_(" ") << _("hours");
			}
		}
	} else if (m > 0) {
		if (m == 1) {
			ap << N_("1 ") << _("minute");
		} else {
			ap << m << N_(" ") << _("minutes");
		}
	} else {
		ap << s << N_(" ") << _("seconds");
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
	size_t size;
	char **strings;
	size_t i;
     
	size = backtrace (array, 200);
	strings = backtrace_symbols (array, size);
     
	if (strings) {
		for (i = 0; i < size && (levels == 0 || i < size_t(levels)); i++) {
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
	stringstream s;
	s << ((v & 0xff0000) >> 16) << N_(".") << ((v & 0xff00) >> 8) << N_(".") << (v & 0xff);
	return s.str ();
}

/** Return a user-readable string summarising the versions of our dependencies */
string
dependency_version_summary ()
{
	stringstream s;
	s << N_("libopenjpeg ") << opj_version () << N_(", ")
	  << N_("libavcodec ") << ffmpeg_version_to_string (avcodec_version()) << N_(", ")
	  << N_("libavfilter ") << ffmpeg_version_to_string (avfilter_version()) << N_(", ")
	  << N_("libavformat ") << ffmpeg_version_to_string (avformat_version()) << N_(", ")
	  << N_("libavutil ") << ffmpeg_version_to_string (avutil_version()) << N_(", ")
	  << N_("libpostproc ") << ffmpeg_version_to_string (postproc_version()) << N_(", ")
	  << N_("libswscale ") << ffmpeg_version_to_string (swscale_version()) << N_(", ")
	  << MagickVersion << N_(", ")
	  << N_("libssh ") << ssh_version (0) << N_(", ")
	  << N_("libdcp ") << libdcp::version << N_(" git ") << libdcp::git_commit;

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
	ofstream f (backtrace_file.string().c_str());
	std::copy(s.begin(), s.end(), std::ostream_iterator<dbg::stack_frame>(f, "\n"));
	return EXCEPTION_CONTINUE_SEARCH;
}
#endif

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
#endif	
	
	avfilter_register_all ();
	
	Ratio::setup_ratios ();
	DCPContentType::setup_dcp_content_types ();
	Scaler::setup_scalers ();
	Filter::setup_filters ();
	SoundProcessor::setup_sound_processors ();

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

void
dcpomatic_setup_gettext_i18n (string lang)
{
#ifdef DCPOMATIC_POSIX
	lang += ".UTF8";
#endif

	if (!lang.empty ()) {
		/* Override our environment language; this is essential on
		   Windows.
		*/
		char cmd[64];
		snprintf (cmd, sizeof(cmd), "LANGUAGE=%s", lang.c_str ());
		putenv (cmd);
		snprintf (cmd, sizeof(cmd), "LANG=%s", lang.c_str ());
		putenv (cmd);
	}

	setlocale (LC_ALL, "");
	textdomain ("libdcpomatic");

#ifdef DCPOMATIC_WINDOWS
	bindtextdomain ("libdcpomatic", mo_path().string().c_str());
	bind_textdomain_codeset ("libdcpomatic", "UTF8");
#endif	

#ifdef DCPOMATIC_POSIX
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

string
md5_digest (void const * data, int size)
{
	MD5_CTX md5_context;
	MD5_Init (&md5_context);
	MD5_Update (&md5_context, data, size);
	unsigned char digest[MD5_DIGEST_LENGTH];
	MD5_Final (digest, &md5_context);
	
	stringstream s;
	for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
		s << std::hex << std::setfill('0') << std::setw(2) << ((int) digest[i]);
	}

	return s.str ();
}

/** @param file File name.
 *  @return MD5 digest of file's contents.
 */
string
md5_digest (boost::filesystem::path file)
{
	ifstream f (file.string().c_str(), std::ios::binary);
	if (!f.good ()) {
		throw OpenFileError (file.string());
	}
	
	f.seekg (0, std::ios::end);
	int bytes = f.tellg ();
	f.seekg (0, std::ios::beg);

	int const buffer_size = 64 * 1024;
	char buffer[buffer_size];

	MD5_CTX md5_context;
	MD5_Init (&md5_context);
	while (bytes > 0) {
		int const t = min (bytes, buffer_size);
		f.read (buffer, t);
		MD5_Update (&md5_context, buffer, t);
		bytes -= t;
	}

	unsigned char digest[MD5_DIGEST_LENGTH];
	MD5_Final (digest, &md5_context);

	stringstream s;
	for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
		s << std::hex << std::setfill('0') << std::setw(2) << ((int) digest[i]);
	}

	return s.str ();
}

static bool
about_equal (float a, float b)
{
	/* A film of F seconds at f FPS will be Ff frames;
	   Consider some delta FPS d, so if we run the same
	   film at (f + d) FPS it will last F(f + d) seconds.

	   Hence the difference in length over the length of the film will
	   be F(f + d) - Ff frames
	    = Ff + Fd - Ff frames
	    = Fd frames
	    = Fd/f seconds
 
	   So if we accept a difference of 1 frame, ie 1/f seconds, we can
	   say that

	   1/f = Fd/f
	ie 1 = Fd
	ie d = 1/F
 
	   So for a 3hr film, ie F = 3 * 60 * 60 = 10800, the acceptable
	   FPS error is 1/F ~= 0.0001 ~= 10-e4
	*/

	return (fabs (a - b) < 1e-4);
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
	, _timeout (timeout)
{
	_deadline.expires_at (boost::posix_time::pos_infin);
	check ();
}

void
Socket::check ()
{
	if (_deadline.expires_at() <= boost::asio::deadline_timer::traits_type::now ()) {
		_socket.close ();
		_deadline.expires_at (boost::posix_time::pos_infin);
	}

	_deadline.async_wait (boost::bind (&Socket::check, this));
}

/** Blocking connect.
 *  @param endpoint End-point to connect to.
 */
void
Socket::connect (boost::asio::ip::basic_resolver_entry<boost::asio::ip::tcp> const & endpoint)
{
	_deadline.expires_from_now (boost::posix_time::seconds (_timeout));
	boost::system::error_code ec = boost::asio::error::would_block;
	_socket.async_connect (endpoint, boost::lambda::var(ec) = boost::lambda::_1);
	do {
		_io_service.run_one();
	} while (ec == boost::asio::error::would_block);

	if (ec || !_socket.is_open ()) {
		throw NetworkError (_("connect timed out"));
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
		throw NetworkError (ec.message ());
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
		throw NetworkError (ec.message ());
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

int
stride_lookup (int c, int const * stride)
{
	return stride[c];
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
	return lexical_cast<int> (v);
}

float
get_required_float (multimap<string, string> const & kv, string k)
{
	string const v = get_required_string (kv, k);
	return lexical_cast<float> (v);
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

	return lexical_cast<int> (i->second);
}

/** Trip an assert if the caller is not in the UI thread */
void
ensure_ui_thread ()
{
	assert (boost::this_thread::get_id() == ui_thread);
}

/** @param v Content video frame.
 *  @param audio_sample_rate Source audio sample rate.
 *  @param frames_per_second Number of video frames per second.
 *  @return Equivalent number of audio frames for `v'.
 */
int64_t
video_frames_to_audio_frames (VideoContent::Frame v, float audio_sample_rate, float frames_per_second)
{
	return ((int64_t) v * audio_sample_rate / frames_per_second);
}

string
audio_channel_name (int c)
{
	assert (MAX_AUDIO_CHANNELS == 6);

	/* TRANSLATORS: these are the names of audio channels; Lfe (sub) is the low-frequency
	   enhancement channel (sub-woofer)./
	*/
	string const channels[] = {
		_("Left"),
		_("Right"),
		_("Centre"),
		_("Lfe (sub)"),
		_("Left surround"),
		_("Right surround"),
	};

	return channels[c];
}

FrameRateConversion::FrameRateConversion (float source, int dcp)
	: skip (false)
	, repeat (false)
	, change_speed (false)
{
	if (fabs (source / 2.0 - dcp) < (fabs (source - dcp))) {
		skip = true;
	} else if (fabs (source * 2 - dcp) < fabs (source - dcp)) {
		repeat = true;
	}

	change_speed = !about_equal (source * factor(), dcp);

	if (!skip && !repeat && !change_speed) {
		description = _("DCP and source have the same rate.\n");
	} else {
		if (skip) {
			description = _("DCP will use every other frame of the source.\n");
		} else if (repeat) {
			description = _("Each source frame will be doubled in the DCP.\n");
		}

		if (change_speed) {
			float const pc = dcp * 100 / (source * factor());
			description += String::compose (_("DCP will run at %1%% of the source speed.\n"), pc);
		}
	}
}

LocaleGuard::LocaleGuard ()
	: _old (0)
{
	char const * old = setlocale (LC_NUMERIC, 0);

        if (old) {
                _old = strdup (old);
                if (strcmp (_old, "C")) {
                        setlocale (LC_NUMERIC, "C");
                }
        }
}

LocaleGuard::~LocaleGuard ()
{
	setlocale (LC_NUMERIC, _old);
	free (_old);
}
