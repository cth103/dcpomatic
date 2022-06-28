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

/** @file src/lib/util.cc
 *  @brief Some utility functions and classes.
 */


#define UNICODE 1


#include "audio_buffers.h"
#include "audio_processor.h"
#include "cinema_sound_processor.h"
#include "compose.hpp"
#include "config.h"
#include "cross.h"
#include "crypto.h"
#include "dcp_content_type.h"
#include "dcpomatic_log.h"
#include "digester.h"
#include "exceptions.h"
#include "ffmpeg_image_proxy.h"
#include "filter.h"
#include "font.h"
#include "image.h"
#include "job.h"
#include "job_manager.h"
#include "ratio.h"
#include "rect.h"
#include "render_text.h"
#include "string_text.h"
#include "text_decoder.h"
#include "util.h"
#include "video_content.h"
#include <dcp/atmos_asset.h>
#include <dcp/decrypted_kdm.h>
#include <dcp/locale_convert.h>
#include <dcp/picture_asset.h>
#include <dcp/raw_convert.h>
#include <dcp/sound_asset.h>
#include <dcp/subtitle_asset.h>
#include <dcp/util.h>
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
extern "C" {
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}
LIBDCP_ENABLE_WARNINGS
#include <curl/curl.h>
#include <glib.h>
#include <pangomm/init.h>
#include <unicode/utypes.h>
#include <unicode/unistr.h>
#include <unicode/translit.h>
#include <boost/algorithm/string.hpp>
#include <boost/range/algorithm/replace_if.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
LIBDCP_DISABLE_WARNINGS
#include <boost/locale.hpp>
LIBDCP_ENABLE_WARNINGS
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


using std::bad_alloc;
using std::cout;
using std::endl;
using std::istream;
using std::list;
using std::make_pair;
using std::make_shared;
using std::map;
using std::min;
using std::ostream;
using std::pair;
using std::set_terminate;
using std::shared_ptr;
using std::string;
using std::vector;
using std::wstring;
using boost::thread;
using boost::optional;
using boost::lexical_cast;
using boost::bad_lexical_cast;
using boost::scoped_array;
using dcp::Size;
using dcp::raw_convert;
using dcp::locale_convert;
using namespace dcpomatic;


/** Path to our executable, required by the stacktrace stuff and filled
 *  in during App::onInit().
 */
string program_name;
bool is_batch_converter = false;
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

	char buffer[64];
	snprintf (buffer, sizeof(buffer), "%d:%02d:%02d", h, m, s);
	return buffer;
}

string
time_to_hmsf (DCPTime time, Frame rate)
{
	Frame f = time.frames_round (rate);
	int s = f / rate;
	f -= (s * rate);
	int m = s / 60;
	s -= m * 60;
	int h = m / 60;
	m -= h * 60;

	char buffer[64];
	snprintf (buffer, sizeof(buffer), "%d:%02d:%02d.%d", h, m, s, static_cast<int>(f));
	return buffer;
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

	string ap;

	bool hours = h > 0;
	bool minutes = h < 6 && m > 0;
	bool seconds = h == 0 && m < 10 && s > 0;

	if (m > 30 && !minutes) {
		/* round up the hours */
		++h;
	}
	if (s > 30 && !seconds) {
		/* round up the minutes */
		++m;
		if (m == 60) {
			m = 0;
			minutes = false;
			++h;
		}
	}

	if (hours) {
		/// TRANSLATORS: h here is an abbreviation for hours
		ap += locale_convert<string>(h) + _("h");

		if (minutes || seconds) {
			ap += N_(" ");
		}
	}

	if (minutes) {
		/// TRANSLATORS: m here is an abbreviation for minutes
		ap += locale_convert<string>(m) + _("m");

		if (seconds) {
			ap += N_(" ");
		}
	}

	if (seconds) {
		/* Seconds */
		/// TRANSLATORS: s here is an abbreviation for seconds
		ap += locale_convert<string>(s) + _("s");
	}

	return ap;
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

LIBDCP_DISABLE_WARNINGS
/** This is called when C signals occur on Windows (e.g. SIGSEGV)
 *  (NOT C++ exceptions!).  We write a backtrace to backtrace_file by dark means.
 *  Adapted from code here: http://spin.atomicobject.com/2013/01/13/exceptions-stack-traces-c/
 */
LONG WINAPI
exception_handler(struct _EXCEPTION_POINTERS * info)
{
	dcp::File f(backtrace_file, "w");
	if (f) {
		fprintf(f.get(), "C-style exception %d\n", info->ExceptionRecord->ExceptionCode);
		f.close();
	}

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
LIBDCP_ENABLE_WARNINGS
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

void
dcpomatic_setup_path_encoding ()
{
#ifdef DCPOMATIC_WINDOWS
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
#endif

#ifdef DCPOMATIC_HAVE_AVREGISTER
LIBDCP_DISABLE_WARNINGS
	av_register_all ();
	avfilter_register_all ();
LIBDCP_ENABLE_WARNINGS
#endif

#ifdef DCPOMATIC_OSX
	/* Add our library directory to the libltdl search path so that
	   xmlsec can find xmlsec1-openssl.
	*/
	auto lib = directory_containing_executable().parent_path();
	lib /= "Frameworks";
	setenv ("LTDL_LIBRARY_PATH", lib.c_str (), 1);
#endif

	set_terminate (terminate);

#ifdef DCPOMATIC_WINDOWS
	putenv ("PANGOCAIRO_BACKEND=fontconfig");
	putenv (String::compose("FONTCONFIG_PATH=%1", resources_path().string()).c_str());
#endif

#ifdef DCPOMATIC_OSX
	setenv ("PANGOCAIRO_BACKEND", "fontconfig", 1);
	setenv ("FONTCONFIG_PATH", resources_path().string().c_str(), 1);
#endif

	Pango::init ();
	dcp::init (libdcp_resources_path());

#if defined(DCPOMATIC_WINDOWS) || defined(DCPOMATIC_OSX)
	/* Render something to fontconfig to create its cache */
	list<StringText> subs;
	dcp::SubtitleString ss(
		optional<string>(), false, false, false, dcp::Colour(), 42, 1, dcp::Time(), dcp::Time(), 0, dcp::HAlign::CENTER, 0, dcp::VAlign::CENTER, dcp::Direction::LTR,
		"Hello dolly", dcp::Effect::NONE, dcp::Colour(), dcp::Time(), dcp::Time(), 0
		);
	subs.push_back (StringText(ss, 0, {}));
	render_text (subs, dcp::Size(640, 480), DCPTime(), 24);
#endif

	Ratio::setup_ratios ();
	PresetColourConversion::setup_colour_conversion_presets ();
	DCPContentType::setup_dcp_content_types ();
	Filter::setup_filters ();
	CinemaSoundProcessor::setup_cinema_sound_processors ();
	AudioProcessor::setup_audio_processors ();

	curl_global_init (CURL_GLOBAL_ALL);

	ui_thread = boost::this_thread::get_id ();

	capture_asdcp_logs ();
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
	textdomain ("libdcpomatic2");

#if defined(DCPOMATIC_WINDOWS) || defined(DCPOMATIC_OSX)
	bindtextdomain ("libdcpomatic2", mo_path().string().c_str());
	bind_textdomain_codeset ("libdcpomatic2", "UTF8");
#endif

#ifdef DCPOMATIC_LINUX
	bindtextdomain ("libdcpomatic2", LINUX_LOCALE_PREFIX);
#endif
}

/** Compute a digest of the first and last `size' bytes of a set of files. */
string
digest_head_tail (vector<boost::filesystem::path> files, boost::uintmax_t size)
{
	boost::scoped_array<char> buffer (new char[size]);
	Digester digester;

	/* Head */
	boost::uintmax_t to_do = size;
	char* p = buffer.get ();
	int i = 0;
	while (i < int64_t (files.size()) && to_do > 0) {
		dcp::File f(files[i], "rb");
		if (!f) {
			throw OpenFileError (files[i].string(), errno, OpenFileError::READ);
		}

		boost::uintmax_t this_time = min (to_do, boost::filesystem::file_size (files[i]));
		f.checked_read(p, this_time);
		p += this_time;
 		to_do -= this_time;

		++i;
	}
	digester.add (buffer.get(), size - to_do);

	/* Tail */
	to_do = size;
	p = buffer.get ();
	i = files.size() - 1;
	while (i >= 0 && to_do > 0) {
		dcp::File f(files[i], "rb");
		if (!f) {
			throw OpenFileError (files[i].string(), errno, OpenFileError::READ);
		}

		boost::uintmax_t this_time = min (to_do, boost::filesystem::file_size (files[i]));
		f.seek(-this_time, SEEK_END);
		f.checked_read(p, this_time);
		p += this_time;
		to_do -= this_time;

		--i;
	}
	digester.add (buffer.get(), size - to_do);

	return digester.get ();
}


string
simple_digest (vector<boost::filesystem::path> paths)
{
	return digest_head_tail(paths, 1000000) + raw_convert<string>(boost::filesystem::file_size(paths.front()));
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
	DCPOMATIC_ASSERT (MAX_DCP_AUDIO_CHANNELS == 16);

	/// TRANSLATORS: these are the names of audio channels; Lfe (sub) is the low-frequency
	/// enhancement channel (sub-woofer).
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
		_("D-BOX primary"),
		_("D-BOX secondary"),
		_("Unused"),
		_("Unused")
	};

	return channels[c];
}

string
short_audio_channel_name (int c)
{
	DCPOMATIC_ASSERT (MAX_DCP_AUDIO_CHANNELS == 16);

	/// TRANSLATORS: these are short names of audio channels; Lfe is the low-frequency
	/// enhancement channel (sub-woofer).  HI is the hearing-impaired audio track and
	/// VI is the visually-impaired audio track (audio describe).  DBP is the D-BOX
	/// primary channel and DBS is the D-BOX secondary channel.
	string const channels[] = {
		_("L"),
		_("R"),
		_("C"),
		_("Lfe"),
		_("Ls"),
		_("Rs"),
		_("HI"),
		_("VI"),
		_("Lc"),
		_("Rc"),
		_("BsL"),
		_("BsR"),
		_("DBP"),
		_("DBS"),
		_("Sign"),
		""
	};

	return channels[c];
}


bool
valid_image_file (boost::filesystem::path f)
{
	if (boost::starts_with (f.leaf().string(), "._")) {
		return false;
	}

	auto ext = f.extension().string();
	transform (ext.begin(), ext.end(), ext.begin(), ::tolower);
	return (
		ext == ".tif" || ext == ".tiff" || ext == ".jpg" || ext == ".jpeg" ||
		ext == ".png" || ext == ".bmp" || ext == ".tga" || ext == ".dpx" ||
		ext == ".j2c" || ext == ".j2k" || ext == ".jp2" || ext == ".exr" ||
		ext == ".jpf" || ext == ".psd"
		);
}

bool
valid_sound_file (boost::filesystem::path f)
{
	if (boost::starts_with (f.leaf().string(), "._")) {
		return false;
	}

	auto ext = f.extension().string();
	transform (ext.begin(), ext.end(), ext.begin(), ::tolower);
	return (ext == ".wav" || ext == ".mp3" || ext == ".aif" || ext == ".aiff");
}

bool
valid_j2k_file (boost::filesystem::path f)
{
	auto ext = f.extension().string();
	transform (ext.begin(), ext.end(), ext.begin(), ::tolower);
	return (ext == ".j2k" || ext == ".j2c" || ext == ".jp2");
}

string
tidy_for_filename (string f)
{
	boost::replace_if (f, boost::is_any_of ("\\/:"), '_');
	return f;
}

dcp::Size
fit_ratio_within (float ratio, dcp::Size full_frame)
{
	if (ratio < full_frame.ratio ()) {
		return dcp::Size (lrintf (full_frame.height * ratio), full_frame.height);
	}

	return dcp::Size (full_frame.width, lrintf (full_frame.width / ratio));
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


static
string
asset_filename (shared_ptr<dcp::Asset> asset, string type, int reel_index, int reel_count, optional<string> summary, string extension)
{
	dcp::NameFormat::Map values;
	values['t'] = type;
	values['r'] = raw_convert<string>(reel_index + 1);
	values['n'] = raw_convert<string>(reel_count);
	if (summary) {
		values['c'] = careful_string_filter(summary.get());
	}
	return Config::instance()->dcp_asset_filename_format().get(values, "_" + asset->id() + extension);
}


string
video_asset_filename (shared_ptr<dcp::PictureAsset> asset, int reel_index, int reel_count, optional<string> summary)
{
	return asset_filename(asset, "j2c", reel_index, reel_count, summary, ".mxf");
}


string
audio_asset_filename (shared_ptr<dcp::SoundAsset> asset, int reel_index, int reel_count, optional<string> summary)
{
	return asset_filename(asset, "pcm", reel_index, reel_count, summary, ".mxf");
}


string
subtitle_asset_filename (shared_ptr<dcp::SubtitleAsset> asset, int reel_index, int reel_count, optional<string> summary, string extension)
{
	return asset_filename(asset, "sub", reel_index, reel_count, summary, extension);
}


string
atmos_asset_filename (shared_ptr<dcp::AtmosAsset> asset, int reel_index, int reel_count, optional<string> summary)
{
	return asset_filename(asset, "atmos", reel_index, reel_count, summary, ".mxf");
}


float
relaxed_string_to_float (string s)
{
	try {
		boost::algorithm::replace_all (s, ",", ".");
		return lexical_cast<float> (s);
	} catch (bad_lexical_cast &) {
		boost::algorithm::replace_all (s, ".", ",");
		return lexical_cast<float> (s);
	}
}

string
careful_string_filter (string s)
{
	/* Filter out `bad' characters which `may' cause problems with some systems (either for DCP name or filename).
	   There's no apparent list of what really is allowed, so this is a guess.
	   Safety first and all that.
	*/

	/* First transliterate using libicu to try to remove accents in a "nice" way */
	auto transliterated = icu::UnicodeString::fromUTF8(icu::StringPiece(s));
	auto status = U_ZERO_ERROR;
	auto transliterator = icu::Transliterator::createInstance("NFD; [:M:] Remove; NFC", UTRANS_FORWARD, status);
	transliterator->transliterate(transliterated);

	/* Some things are missed by ICU's transliterator */
	std::map<wchar_t, wchar_t> replacements = {
		{ L'ł',	 L'l' },
		{ L'Ł',	 L'L' }
	};

	icu::UnicodeString transliterated_more;
	for (int i = 0; i < transliterated.length(); ++i) {
		auto replacement = replacements.find(transliterated[i]);
		if (replacement != replacements.end()) {
			transliterated_more += replacement->second;
		} else {
			transliterated_more += transliterated[i];
		}
	}

	/* Then remove anything that's not in a very limited character set */
	wstring out;
	wstring const allowed = L"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_%.+";
	for (int i = 0; i < transliterated_more.length(); ++i) {
		wchar_t c = transliterated_more[i];
		if (allowed.find(c) != string::npos) {
			out += c;
		}
	}

	return boost::locale::conv::utf_to_utf<char>(out);
}

/** @param mapped List of mapped audio channels from a Film.
 *  @param channels Total number of channels in the Film.
 *  @return First: number of non-LFE soundtrack channels (L/R/C/Ls/Rs/Lc/Rc/Bsl/Bsr), second: number of LFE channels.
 */
pair<int, int>
audio_channel_types (list<int> mapped, int channels)
{
	int non_lfe = 0;
	int lfe = 0;

	for (auto i: mapped) {
		if (i >= channels) {
			/* This channel is mapped but is not included in the DCP */
			continue;
		}

		switch (static_cast<dcp::Channel>(i)) {
		case dcp::Channel::LFE:
			++lfe;
			break;
		case dcp::Channel::LEFT:
		case dcp::Channel::RIGHT:
		case dcp::Channel::CENTRE:
		case dcp::Channel::LS:
		case dcp::Channel::RS:
		case dcp::Channel::BSL:
		case dcp::Channel::BSR:
			++non_lfe;
			break;
		case dcp::Channel::HI:
		case dcp::Channel::VI:
		case dcp::Channel::MOTION_DATA:
		case dcp::Channel::SYNC_SIGNAL:
		case dcp::Channel::SIGN_LANGUAGE:
		case dcp::Channel::CHANNEL_COUNT:
			break;
		}
	}

	return make_pair (non_lfe, lfe);
}

shared_ptr<AudioBuffers>
remap (shared_ptr<const AudioBuffers> input, int output_channels, AudioMapping map)
{
	auto mapped = make_shared<AudioBuffers>(output_channels, input->frames());
	mapped->make_silent ();

	int to_do = min (map.input_channels(), input->channels());

	for (int i = 0; i < to_do; ++i) {
		for (int j = 0; j < mapped->channels(); ++j) {
			if (map.get(i, j) > 0) {
				mapped->accumulate_channel(
					input.get(),
					i,
					j,
					map.get(i, j)
					);
			}
		}
	}

	return mapped;
}

Eyes
increment_eyes (Eyes e)
{
	if (e == Eyes::LEFT) {
		return Eyes::RIGHT;
	}

	return Eyes::LEFT;
}


size_t
utf8_strlen (string s)
{
	size_t const len = s.length ();
	int N = 0;
	for (size_t i = 0; i < len; ++i) {
		unsigned char c = s[i];
		if ((c & 0xe0) == 0xc0) {
			++i;
		} else if ((c & 0xf0) == 0xe0) {
			i += 2;
		} else if ((c & 0xf8) == 0xf0) {
			i += 3;
		}
		++N;
	}
	return N;
}

string
day_of_week_to_string (boost::gregorian::greg_weekday d)
{
	switch (d.as_enum()) {
	case boost::date_time::Sunday:
		return _("Sunday");
	case boost::date_time::Monday:
		return _("Monday");
	case boost::date_time::Tuesday:
		return _("Tuesday");
	case boost::date_time::Wednesday:
		return _("Wednesday");
	case boost::date_time::Thursday:
		return _("Thursday");
	case boost::date_time::Friday:
		return _("Friday");
	case boost::date_time::Saturday:
		return _("Saturday");
	}

	return d.as_long_string ();
}

/** @param size Size of picture that the subtitle will be overlaid onto */
void
emit_subtitle_image (ContentTimePeriod period, dcp::SubtitleImage sub, dcp::Size size, shared_ptr<TextDecoder> decoder)
{
	/* XXX: this is rather inefficient; decoding the image just to get its size */
	FFmpegImageProxy proxy (sub.png_image());
	auto image = proxy.image(Image::Alignment::PADDED).image;
	/* set up rect with height and width */
	dcpomatic::Rect<double> rect(0, 0, image->size().width / double(size.width), image->size().height / double(size.height));

	/* add in position */

	switch (sub.h_align()) {
	case dcp::HAlign::LEFT:
		rect.x += sub.h_position();
		break;
	case dcp::HAlign::CENTER:
		rect.x += 0.5 + sub.h_position() - rect.width / 2;
		break;
	case dcp::HAlign::RIGHT:
		rect.x += 1 - sub.h_position() - rect.width;
		break;
	}

	switch (sub.v_align()) {
	case dcp::VAlign::TOP:
		rect.y += sub.v_position();
		break;
	case dcp::VAlign::CENTER:
		rect.y += 0.5 + sub.v_position() - rect.height / 2;
		break;
	case dcp::VAlign::BOTTOM:
		rect.y += 1 - sub.v_position() - rect.height;
		break;
	}

	decoder->emit_bitmap (period, image, rect);
}

bool
show_jobs_on_console (bool progress)
{
	bool first = true;
	bool error = false;
	while (true) {

		dcpomatic_sleep_seconds (5);

		auto jobs = JobManager::instance()->get();

		if (!first && progress) {
			for (size_t i = 0; i < jobs.size(); ++i) {
				cout << "\033[1A\033[2K";
			}
			cout.flush ();
		}

		first = false;

		for (auto i: jobs) {
			if (progress) {
				cout << i->name();
				if (!i->sub_name().empty()) {
					cout << "; " << i->sub_name();
				}
				cout << ": ";

				if (i->progress ()) {
					cout << i->status() << "			    \n";
				} else {
					cout << ": Running	     \n";
				}
			}

			if (!progress && i->finished_in_error()) {
				/* We won't see this error if we haven't been showing progress,
				   so show it now.
				*/
				cout << i->status() << "\n";
			}

			if (i->finished_in_error()) {
				error = true;
			}
		}

		if (!JobManager::instance()->work_to_do()) {
			break;
		}
	}

	return error;
}

/** XXX: could use mmap? */
void
copy_in_bits (boost::filesystem::path from, boost::filesystem::path to, std::function<void (float)> progress)
{
	dcp::File f(from, "rb");
	if (!f) {
		throw OpenFileError (from, errno, OpenFileError::READ);
	}
	dcp::File t(to, "wb");
	if (!t) {
		throw OpenFileError (to, errno, OpenFileError::WRITE);
	}

	/* on the order of a second's worth of copying */
	boost::uintmax_t const chunk = 20 * 1024 * 1024;

	std::vector<uint8_t> buffer(chunk);

	boost::uintmax_t const total = boost::filesystem::file_size (from);
	boost::uintmax_t remaining = total;

	while (remaining) {
		boost::uintmax_t this_time = min (chunk, remaining);
		size_t N = f.read(buffer.data(), 1, chunk);
		if (N < this_time) {
			throw ReadFileError (from, errno);
		}

		N = t.write(buffer.data(), 1, this_time);
		if (N < this_time) {
			throw WriteFileError (to, errno);
		}

		progress (1 - float(remaining) / total);
		remaining -= this_time;
	}
}


dcp::Size
scale_for_display (dcp::Size s, dcp::Size display_container, dcp::Size film_container, PixelQuanta quanta)
{
	/* Now scale it down if the display container is smaller than the film container */
	if (display_container != film_container) {
		float const scale = min (
			float (display_container.width) / film_container.width,
			float (display_container.height) / film_container.height
			);

		s.width = lrintf (s.width * scale);
		s.height = lrintf (s.height * scale);
		s = quanta.round (s);
	}

	return s;
}


dcp::DecryptedKDM
decrypt_kdm_with_helpful_error (dcp::EncryptedKDM kdm)
{
	try {
		return dcp::DecryptedKDM (kdm, Config::instance()->decryption_chain()->key().get());
	} catch (dcp::KDMDecryptionError& e) {
		/* Try to flesh out the error a bit */
		auto const kdm_subject_name = kdm.recipient_x509_subject_name();
		bool on_chain = false;
		auto dc = Config::instance()->decryption_chain();
		for (auto i: dc->root_to_leaf()) {
			if (i.subject() == kdm_subject_name) {
				on_chain = true;
			}
		}
		if (!on_chain) {
			throw KDMError (_("This KDM was not made for DCP-o-matic's decryption certificate."), e.what());
		} else if (kdm_subject_name != dc->leaf().subject()) {
			throw KDMError (_("This KDM was made for DCP-o-matic but not for its leaf certificate."), e.what());
		} else {
			throw;
		}
	}
}


boost::filesystem::path
default_font_file ()
{
	boost::filesystem::path liberation_normal;
	try {
		liberation_normal = resources_path() / "LiberationSans-Regular.ttf";
		if (!boost::filesystem::exists (liberation_normal)) {
			/* Hack for unit tests */
			liberation_normal = resources_path() / "fonts" / "LiberationSans-Regular.ttf";
		}
	} catch (boost::filesystem::filesystem_error& e) {

	}

	if (!boost::filesystem::exists(liberation_normal)) {
		liberation_normal = "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf";
	}
	if (!boost::filesystem::exists(liberation_normal)) {
		liberation_normal = "/usr/share/fonts/liberation-sans/LiberationSans-Regular.ttf";
	}

	return liberation_normal;
}


string
to_upper (string s)
{
	transform (s.begin(), s.end(), s.begin(), ::toupper);
	return s;
}


/* Set to 1 to print the IDs of some of our threads to stdout on creation */
#define DCPOMATIC_DEBUG_THREADS 0

#if DCPOMATIC_DEBUG_THREADS
void
start_of_thread (string name)
{
	std::cout << "THREAD:" << name << ":" << std::hex << pthread_self() << "\n";
}
#else
void
start_of_thread (string)
{

}
#endif


class LogSink : public Kumu::ILogSink
{
public:
	LogSink () {}
	LogSink (LogSink const&) = delete;
	LogSink& operator= (LogSink const&) = delete;

	void WriteEntry(const Kumu::LogEntry& entry) override {
		Kumu::AutoMutex L(m_lock);
		WriteEntryToListeners(entry);
		if (entry.TestFilter(m_filter)) {
			string buffer;
			entry.CreateStringWithOptions(buffer, m_options);
			LOG_GENERAL("asdcplib: %1", buffer);
		}
	}
};


void
capture_asdcp_logs ()
{
	static LogSink log_sink;
	Kumu::SetDefaultLogSink(&log_sink);
}

