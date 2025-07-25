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
#include "config.h"
#include "constants.h"
#include "cross.h"
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
#include "variant.h"
#include "video_content.h"
#include <dcp/atmos_asset.h>
#include <dcp/decrypted_kdm.h>
#include <dcp/file.h>
#include <dcp/filesystem.h>
#include <dcp/locale_convert.h>
#include <dcp/mpeg2_picture_asset.h>
#include <dcp/picture_asset.h>
#include <dcp/raw_convert.h>
#include <dcp/scope_guard.h>
#include <dcp/sound_asset.h>
#include <dcp/text_asset.h>
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
#include <unicode/brkiter.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/c_local_time_adjustor.hpp>
#include <boost/range/algorithm/replace_if.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
LIBDCP_DISABLE_WARNINGS
#include <boost/locale.hpp>
LIBDCP_ENABLE_WARNINGS
#ifdef DCPOMATIC_WINDOWS
#include <dbghelp.h>
#endif
#include <signal.h>
#include <climits>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <numeric>
#include <stdexcept>
#ifdef DCPOMATIC_POSIX
#include <execinfo.h>
#include <cxxabi.h>
#endif

#include "i18n.h"


using std::bad_alloc;
using std::cout;
using std::dynamic_pointer_cast;
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
using dcp::Size;
using dcp::raw_convert;
using dcp::locale_convert;
using namespace dcpomatic;


/** Path to our executable, required by the stacktrace stuff and filled
 *  in during App::onInit().
 */
string program_name;
bool is_batch_converter = false;
bool running_tests = false;
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
seconds_to_hms(int s)
{
	int m = s / 60;
	s -= (m * 60);
	int h = m / 60;
	m -= (h * 60);

	char buffer[64];
	snprintf(buffer, sizeof(buffer), "%d:%02d:%02d", h, m, s);
	return buffer;
}

string
time_to_hmsf(DCPTime time, Frame rate)
{
	Frame f = time.frames_round(rate);
	int s = f / rate;
	f -= (s * rate);
	int m = s / 60;
	s -= m * 60;
	int h = m / 60;
	m -= h * 60;

	char buffer[64];
	snprintf(buffer, sizeof(buffer), "%d:%02d:%02d.%d", h, m, s, static_cast<int>(f));
	return buffer;
}

/** @param s Number of seconds.
 *  @return String containing an approximate description of s (e.g. "about 2 hours")
 */
string
seconds_to_approximate_hms(int s)
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
seconds(struct timeval t)
{
	return t.tv_sec + (double(t.tv_usec) / 1e6);
}

#ifdef DCPOMATIC_WINDOWS

/** Resolve symbol name and source location given the path to the executable */
int
addr2line(void const * const addr)
{
	char addr2line_cmd[512] = { 0 };
	sprintf(addr2line_cmd, "addr2line -f -p -e %.256s %p > %s", program_name.c_str(), addr, backtrace_file.string().c_str());
	std::cout << addr2line_cmd << "\n";
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
		SymInitialize(GetCurrentProcess(), 0, true);

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
			StackWalk(
				IMAGE_FILE_MACHINE_I386,
				GetCurrentProcess(),
				GetCurrentThread(),
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
		addr2line((void *) info->ContextRecord->Rip);
#else
		addr2line((void *) info->ContextRecord->Eip);
#endif
	}

	return EXCEPTION_CONTINUE_SEARCH;
}
LIBDCP_ENABLE_WARNINGS
#endif

void
set_backtrace_file(boost::filesystem::path p)
{
	backtrace_file = p;
}

/** This is called when there is an unhandled exception.  Any
 *  backtrace in this function is useless on Windows as the stack has
 *  already been unwound from the throw; we have the gdb wrap hack to
 *  cope with that.
 */
void
terminate()
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
dcpomatic_setup_path_encoding()
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
	std::locale::global(boost::locale::generator().generate(""));
	boost::filesystem::path::imbue(std::locale());
#endif
}


class LogSink : public Kumu::ILogSink
{
public:
	LogSink() {}
	LogSink(LogSink const&) = delete;
	LogSink& operator=(LogSink const&) = delete;

	void WriteEntry(const Kumu::LogEntry& entry) override {
		Kumu::AutoMutex L(m_lock);
		WriteEntryToListeners(entry);
		if (entry.TestFilter(m_filter)) {
			string buffer;
			entry.CreateStringWithOptions(buffer, m_options);
			LOG_GENERAL("asdcplib: {}", buffer);
		}
	}
};


void
capture_asdcp_logs()
{
	static LogSink log_sink;
	Kumu::SetDefaultLogSink(&log_sink);
}


static
void
ffmpeg_log_callback(void* ptr, int level, const char* fmt, va_list vl)
{
	if (level > AV_LOG_WARNING) {
		return;
	}

	char line[1024];
	static int prefix = 0;
	av_log_format_line(ptr, level, fmt, vl, line, sizeof(line), &prefix);
	string str(line);
	boost::algorithm::trim(str);
	dcpomatic_log->log(fmt::format("FFmpeg: {}", str), LogEntry::TYPE_GENERAL);
}


void
capture_ffmpeg_logs()
{
	av_log_set_callback(ffmpeg_log_callback);
}


/** Call the required functions to set up DCP-o-matic's static arrays, etc.
 *  Must be called from the UI thread, if there is one.
 */
void
dcpomatic_setup()
{
#ifdef DCPOMATIC_WINDOWS
	boost::filesystem::path p = g_get_user_config_dir();
	p /= "backtrace.txt";
	set_backtrace_file(p);
	SetUnhandledExceptionFilter(exception_handler);
#endif

#ifdef DCPOMATIC_GROK
	/* This makes grok support work with CUDA 12.2 */
	setenv("CUDA_MODULE_LOADING", "EAGER", 1);
#endif

#ifdef DCPOMATIC_HAVE_AVREGISTER
LIBDCP_DISABLE_WARNINGS
	av_register_all();
	avfilter_register_all();
LIBDCP_ENABLE_WARNINGS
#endif

#ifdef DCPOMATIC_OSX
	/* Add our library directory to the libltdl search path so that
	   xmlsec can find xmlsec1-openssl.
	*/
	auto lib = directory_containing_executable().parent_path();
	lib /= "Frameworks";
	setenv("LTDL_LIBRARY_PATH", lib.c_str(), 1);
#endif

	set_terminate(terminate);

#ifdef DCPOMATIC_WINDOWS
	putenv("PANGOCAIRO_BACKEND=fontconfig");
	if (running_tests) {
		putenv("FONTCONFIG_PATH=fonts");
	} else {
		putenv(fmt::format("FONTCONFIG_PATH={}", resources_path().string()).c_str());
	}
#endif

#ifdef DCPOMATIC_OSX
	setenv("PANGOCAIRO_BACKEND", "fontconfig", 1);
	boost::filesystem::path fontconfig;
	if (running_tests) {
		fontconfig = directory_containing_executable().parent_path().parent_path() / "fonts";
	} else {
		fontconfig = resources_path();
	}
	setenv("FONTCONFIG_PATH", fontconfig.string().c_str(), 1);
#endif

	Pango::init();
	dcp::init(libdcp_resources_path());

#if defined(DCPOMATIC_WINDOWS) || defined(DCPOMATIC_OSX)
	/* Render something to fontconfig to create its cache */
	vector<StringText> subs;
	dcp::TextString ss(
		optional<string>(), false, false, false, dcp::Colour(), 42, 1, dcp::Time(), dcp::Time(), 0, dcp::HAlign::CENTER, 0, dcp::VAlign::CENTER, 0,
		vector<dcp::Text::VariableZPosition>(), dcp::Direction::LTR, "Hello dolly", dcp::Effect::NONE, dcp::Colour(), dcp::Time(), dcp::Time(), 0, std::vector<dcp::Ruby>()
		);
	subs.push_back(StringText(ss, 0, make_shared<dcpomatic::Font>("foo"), dcp::SubtitleStandard::SMPTE_2014));
	render_text(subs, dcp::Size(640, 480), DCPTime(), 24);
#endif

#ifdef DCPOMATIC_WINDOWS
	putenv("OPENSSL_ENABLE_SHA1_SIGNATURES=1");
#else
	setenv("OPENSSL_ENABLE_SHA1_SIGNATURES", "1", 1);
#endif

	Ratio::setup_ratios();
	PresetColourConversion::setup_colour_conversion_presets();
	DCPContentType::setup_dcp_content_types();
	Filter::setup_filters();
	CinemaSoundProcessor::setup_cinema_sound_processors();
	AudioProcessor::setup_audio_processors();

	curl_global_init(CURL_GLOBAL_ALL);

	ui_thread = boost::this_thread::get_id();

	capture_asdcp_logs();
	capture_ffmpeg_logs();
}


/** Compute a digest of the first and last `size' bytes of a set of files. */
string
digest_head_tail(vector<boost::filesystem::path> files, boost::uintmax_t size)
{
	boost::scoped_array<char> buffer(new char[size]);
	Digester digester;

	/* Head */
	boost::uintmax_t to_do = size;
	char* p = buffer.get();
	int i = 0;
	while (i < int64_t(files.size()) && to_do > 0) {
		dcp::File f(files[i], "rb");
		if (!f) {
			throw OpenFileError(files[i].string(), f.open_error(), OpenFileError::READ);
		}

		auto this_time = min(to_do, dcp::filesystem::file_size(files[i]));
		f.checked_read(p, this_time);
		p += this_time;
 		to_do -= this_time;

		++i;
	}
	digester.add(buffer.get(), size - to_do);

	/* Tail */
	to_do = size;
	p = buffer.get();
	i = files.size() - 1;
	while (i >= 0 && to_do > 0) {
		dcp::File f(files[i], "rb");
		if (!f) {
			throw OpenFileError(files[i].string(), f.open_error(), OpenFileError::READ);
		}

		auto this_time = min(to_do, dcp::filesystem::file_size(files[i]));
		f.seek(-this_time, SEEK_END);
		f.checked_read(p, this_time);
		p += this_time;
		to_do -= this_time;

		--i;
	}
	digester.add(buffer.get(), size - to_do);

	return digester.get();
}


string
simple_digest(vector<boost::filesystem::path> paths)
{
	DCP_ASSERT(!paths.empty());
	return digest_head_tail(paths, 1000000) + fmt::to_string(dcp::filesystem::file_size(paths.front()));
}


/** Trip an assert if the caller is not in the UI thread */
void
ensure_ui_thread()
{
	DCPOMATIC_ASSERT(boost::this_thread::get_id() == ui_thread);
}

string
audio_channel_name(int c)
{
	DCPOMATIC_ASSERT(MAX_DCP_AUDIO_CHANNELS == 16);

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
short_audio_channel_name(int c)
{
	DCPOMATIC_ASSERT(MAX_DCP_AUDIO_CHANNELS == 16);

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
		_("9"),
		_("10"),
		_("BsL"),
		_("BsR"),
		_("DBP"),
		_("DBS"),
		_("Sign"),
		_("16")
	};

	return channels[c];
}


bool
valid_image_file(boost::filesystem::path f)
{
	if (boost::starts_with(f.filename().string(), "._")) {
		return false;
	}

	auto ext = f.extension().string();
	transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
	return (
		ext == ".tif" || ext == ".tiff" || ext == ".jpg" || ext == ".jpeg" ||
		ext == ".png" || ext == ".bmp" || ext == ".tga" || ext == ".dpx" ||
		ext == ".j2c" || ext == ".j2k" || ext == ".jp2" || ext == ".exr" ||
		ext == ".jpf" || ext == ".psd" || ext == ".webp"
		);
}

bool
valid_sound_file(boost::filesystem::path f)
{
	if (boost::starts_with(f.filename().string(), "._")) {
		return false;
	}

	auto ext = f.extension().string();
	transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
	return (ext == ".wav" || ext == ".mp3" || ext == ".aif" || ext == ".aiff");
}

bool
valid_j2k_file(boost::filesystem::path f)
{
	auto ext = f.extension().string();
	transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
	return (ext == ".j2k" || ext == ".j2c" || ext == ".jp2");
}

string
tidy_for_filename(string f)
{
	boost::replace_if(f, boost::is_any_of("\\/:"), '_');
	return f;
}

dcp::Size
fit_ratio_within(float ratio, dcp::Size full_frame)
{
	if (ratio < full_frame.ratio()) {
		return dcp::Size(lrintf(full_frame.height * ratio), full_frame.height);
	}

	return dcp::Size(full_frame.width, lrintf(full_frame.width / ratio));
}

static
string
asset_filename(shared_ptr<dcp::Asset> asset, string type, int reel_index, int reel_count, optional<string> summary, string extension)
{
	dcp::NameFormat::Map values;
	values['t'] = type;
	values['r'] = fmt::to_string(reel_index + 1);
	values['n'] = fmt::to_string(reel_count);
	if (summary) {
		values['c'] = summary.get();
	}
	return careful_string_filter(Config::instance()->dcp_asset_filename_format().get(values, "_" + asset->id() + extension));
}


string
video_asset_filename(shared_ptr<dcp::PictureAsset> asset, int reel_index, int reel_count, optional<string> summary)
{
	string type = dynamic_pointer_cast<dcp::MPEG2PictureAsset>(asset) ? "mpeg2" : "j2c";
	return asset_filename(asset, type, reel_index, reel_count, summary, ".mxf");
}


string
audio_asset_filename(shared_ptr<dcp::SoundAsset> asset, int reel_index, int reel_count, optional<string> summary)
{
	return asset_filename(asset, "pcm", reel_index, reel_count, summary, ".mxf");
}


string
subtitle_asset_filename(shared_ptr<dcp::TextAsset> asset, int reel_index, int reel_count, optional<string> summary, string extension)
{
	return asset_filename(asset, "sub", reel_index, reel_count, summary, extension);
}


string
atmos_asset_filename(shared_ptr<dcp::AtmosAsset> asset, int reel_index, int reel_count, optional<string> summary)
{
	return asset_filename(asset, "atmos", reel_index, reel_count, summary, ".mxf");
}


string
careful_string_filter(string s, wstring allowed)
{
	/* Filter out `bad' characters which `may' cause problems with some systems (either for DCP name or filename).
	 * I don't know of a list of what really is allowed, so this is a guess.
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
			transliterated_more += static_cast<UChar32>(replacement->second);
		} else {
			transliterated_more += transliterated[i];
		}
	}

	/* Then remove anything that's not in a very limited character set */
	wstring out;
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
audio_channel_types(list<int> mapped, int channels)
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
		case dcp::Channel::LC:
		case dcp::Channel::RC:
		case dcp::Channel::HI:
		case dcp::Channel::VI:
		case dcp::Channel::MOTION_DATA:
		case dcp::Channel::SYNC_SIGNAL:
		case dcp::Channel::SIGN_LANGUAGE:
		case dcp::Channel::CHANNEL_COUNT:
			break;
		}
	}

	return make_pair(non_lfe, lfe);
}

shared_ptr<AudioBuffers>
remap(shared_ptr<const AudioBuffers> input, int output_channels, AudioMapping map)
{
	auto mapped = make_shared<AudioBuffers>(output_channels, input->frames());
	mapped->make_silent();

	int to_do = min(map.input_channels(), input->channels());

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


size_t
utf8_strlen(string s)
{
	size_t const len = s.length();
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


/** @param size Size of picture that the subtitle will be overlaid onto */
void
emit_subtitle_image(ContentTimePeriod period, dcp::TextImage sub, dcp::Size size, shared_ptr<TextDecoder> decoder)
{
	/* XXX: this is rather inefficient; decoding the image just to get its size */
	FFmpegImageProxy proxy(sub.png_image());
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

	decoder->emit_bitmap(period, image, rect);
}


/** XXX: could use mmap? */
void
copy_in_bits(boost::filesystem::path from, boost::filesystem::path to, std::function<void (float)> progress)
{
	dcp::File f(from, "rb");
	if (!f) {
		throw OpenFileError(from, f.open_error(), OpenFileError::READ);
	}
	dcp::File t(to, "wb");
	if (!t) {
		throw OpenFileError(to, t.open_error(), OpenFileError::WRITE);
	}

	/* on the order of a second's worth of copying */
	boost::uintmax_t const chunk = 20 * 1024 * 1024;

	std::vector<uint8_t> buffer(chunk);

	auto const total = dcp::filesystem::file_size(from);
	boost::uintmax_t remaining = total;

	while (remaining) {
		boost::uintmax_t this_time = min(chunk, remaining);
		size_t N = f.read(buffer.data(), 1, chunk);
		if (N < this_time) {
			throw ReadFileError(from, errno);
		}

		N = t.write(buffer.data(), 1, this_time);
		if (N < this_time) {
			throw WriteFileError(to, errno);
		}

		progress(1 - float(remaining) / total);
		remaining -= this_time;
	}
}


dcp::Size
scale_for_display(dcp::Size s, dcp::Size display_container, dcp::Size film_container, PixelQuanta quanta)
{
	/* Now scale it down if the display container is smaller than the film container */
	if (display_container != film_container) {
		float const scale = min(
			float(display_container.width) / film_container.width,
			float(display_container.height) / film_container.height
			);

		s.width = lrintf(s.width * scale);
		s.height = lrintf(s.height * scale);
		s = quanta.round(s);
	}

	return s;
}


dcp::DecryptedKDM
decrypt_kdm_with_helpful_error(dcp::EncryptedKDM kdm)
{
	try {
		return dcp::DecryptedKDM(kdm, Config::instance()->decryption_chain()->key().get());
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
			throw KDMError(variant::insert_dcpomatic(_("This KDM was not made for {}'s decryption certificate.")), e.what());
		} else if (kdm_subject_name != dc->leaf().subject()) {
			throw KDMError(variant::insert_dcpomatic(_("This KDM was made for {} but not for its leaf certificate.")), e.what());
		} else {
			throw;
		}
	}
}


boost::filesystem::path
default_font_file()
{
	if (running_tests) {
		auto const liberation = directory_containing_executable().parent_path().parent_path() / "fonts" / "LiberationSans-Regular.ttf";
		DCPOMATIC_ASSERT(dcp::filesystem::exists(liberation));
		return liberation;
	}

#ifdef DCPOMATIC_DEBUG
	return directory_containing_executable().parent_path().parent_path().parent_path() / "fonts" / "LiberationSans-Regular.ttf";
#endif

	return resources_path() / "LiberationSans-Regular.ttf";
}


/* Set to 1 to print the IDs of some of our threads to stdout on creation */
#define DCPOMATIC_DEBUG_THREADS 0

#if DCPOMATIC_DEBUG_THREADS
void
start_of_thread(string name)
{
	std::cout << "THREAD:" << name << ":" << std::hex << pthread_self() << "\n";
}
#else
void
start_of_thread(string)
{

}
#endif


string
error_details(boost::system::error_code ec)
{
	return fmt::format("{}:{}:{}", ec.category().name(), ec.value(), ec.message());
}


bool
contains_assetmap(boost::filesystem::path dir)
{
	return dcp::filesystem::is_regular_file(dir / "ASSETMAP") || dcp::filesystem::is_regular_file(dir / "ASSETMAP.xml");
}


string
word_wrap(string input, int columns)
{
	icu::Locale locale;
	UErrorCode status = U_ZERO_ERROR;
	auto iter = icu::BreakIterator::createLineInstance(locale, status);
	dcp::ScopeGuard sg = [iter]() { delete iter; };
	if (U_FAILURE(status)) {
		return input;
	}

	auto input_icu = icu::UnicodeString::fromUTF8(icu::StringPiece(input));
	iter->setText(input_icu);

	int position = 0;
	string output;
	while (position < input_icu.length()) {
		int end_of_line = iter->preceding(position + columns + 1);
		icu::UnicodeString line;
		if (end_of_line <= position) {
			/* There's no good line-break position; just break in the middle of a word */
			line = input_icu.tempSubString(position, columns);
			position += columns;
		} else {
			line = input_icu.tempSubString(position, end_of_line - position);
			position = end_of_line;
		}
		line.toUTF8String(output);
		output += "\n";
	}

	return output;
}


#ifdef DCPOMATIC_GROK
void
setup_grok_library_path()
{
	static std::string old_path;
	if (old_path.empty()) {
		if (auto const old = getenv("LD_LIBRARY_PATH")) {
			old_path = old;
		}
	}
	auto const grok = Config::instance()->grok();
	if (grok.binary_location.empty()) {
		setenv("LD_LIRARY_PATH", old_path.c_str(), 1);
		return;
	}

	std::string new_path = old_path;
	if (!new_path.empty()) {
		new_path += ":";
	}
	new_path += grok.binary_location.string();

	setenv("LD_LIBRARY_PATH", new_path.c_str(), 1);
}
#endif


string
screen_names_to_string(vector<string> names)
{
	if (names.empty()) {
		return {};
	}

	auto number = [](string const& s) {
		return s.find_first_not_of("0123456789") == string::npos;
	};

	if (std::find_if(names.begin(), names.end(), [number](string const& s) { return !number(s); }) == names.end()) {
		std::sort(names.begin(), names.end(), [](string const& a, string const& b) {
			return dcp::raw_convert<int>(a) < dcp::raw_convert<int>(b);
		});
	} else {
		std::sort(names.begin(), names.end());
	}

	string result;
	for (auto const& name: names) {
		result += name + ", ";
	}

	return result.substr(0, result.length() - 2);
}


string
report_problem()
{
	return fmt::format(_("Please report this problem by using Help -> Report a problem or via email to {}"), variant::report_problem_email());
}


string
join_strings(vector<string> const& in, string const& separator)
{
	if (in.empty()) {
		return {};
	}

	return std::accumulate(std::next(in.begin()), in.end(), in.front(), [separator](string a, string b) {
		return a + separator + b;
	});
}


string
rfc_2822_date(time_t time)
{
	auto const utc_now = boost::posix_time::second_clock::universal_time();
	auto const local_now = boost::date_time::c_local_adjustor<boost::posix_time::ptime>::utc_to_local(utc_now);
	auto const offset = local_now - utc_now;

	auto const hours = int(abs(offset.hours()));

	auto tm = localtime(&time);

	/* I tried using %z in the time formatter but it gave results like "Pacific Standard Time" instead +0800 on Windows */
	return fmt::format("{:%a, %d %b %Y %H:%M:%S} {}{:02d}{:02d}", *tm, offset.hours() >= 0 ? "+" : "-", hours, int(offset.minutes()));
}


bool
paths_exist(vector<boost::filesystem::path> const& paths)
{
	return std::all_of(paths.begin(), paths.end(), [](boost::filesystem::path const& path) { return dcp::filesystem::exists(path); });
}

