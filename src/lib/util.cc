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
#ifdef DVDOMATIC_POSIX
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
#include "format.h"
#include "dcp_content_type.h"
#include "filter.h"
#include "sound_processor.h"
#include "config.h"

#include "i18n.h"

using namespace std;
using namespace boost;
using libdcp::Size;

thread::id ui_thread;

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

#ifdef DVDOMATIC_POSIX
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
			out << N_("  ") << demangle (strings[i]) << endl;
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

/** Call the required functions to set up DVD-o-matic's static arrays, etc.
 *  Must be called from the UI thread, if there is one.
 */
void
dvdomatic_setup ()
{
	bindtextdomain ("libdvdomatic", LOCALE_PREFIX);
	setlocale (LC_ALL, "");
	
	avfilter_register_all ();
	
	Format::setup_formats ();
	DCPContentType::setup_dcp_content_types ();
	Scaler::setup_scalers ();
	Filter::setup_filters ();
	SoundProcessor::setup_sound_processors ();

	ui_thread = this_thread::get_id ();
}

/** @param start Start position for the crop within the image.
 *  @param size Size of the cropped area.
 *  @return FFmpeg crop filter string.
 */
string
crop_string (Position start, libdcp::Size size)
{
	stringstream s;
	s << N_("crop=") << size.width << N_(":") << size.height << N_(":") << start.x << N_(":") << start.y;
	return s.str ();
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
		s << hex << setfill('0') << setw(2) << ((int) digest[i]);
	}

	return s.str ();
}

/** @param file File name.
 *  @return MD5 digest of file's contents.
 */
string
md5_digest (string file)
{
	ifstream f (file.c_str(), ios::binary);
	if (!f.good ()) {
		throw OpenFileError (file);
	}
	
	f.seekg (0, ios::end);
	int bytes = f.tellg ();
	f.seekg (0, ios::beg);

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
		s << hex << setfill('0') << setw(2) << ((int) digest[i]);
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

class FrameRateCandidate
{
public:
	FrameRateCandidate (float source_, int dcp_)
		: source (source_)
		, dcp (dcp_)
	{}

	float source;
	int dcp;
};

int
best_dcp_frame_rate (float source_fps)
{
	list<int> const allowed_dcp_frame_rates = Config::instance()->allowed_dcp_frame_rates ();

	/* Work out what rates we could manage, including those achieved by using skip / repeat. */
	list<FrameRateCandidate> candidates;

	/* Start with the ones without skip / repeat so they will get matched in preference to skipped/repeated ones */
	for (list<int>::const_iterator i = allowed_dcp_frame_rates.begin(); i != allowed_dcp_frame_rates.end(); ++i) {
		candidates.push_back (FrameRateCandidate (*i, *i));
	}

	/* Then the skip/repeat ones */
	for (list<int>::const_iterator i = allowed_dcp_frame_rates.begin(); i != allowed_dcp_frame_rates.end(); ++i) {
		candidates.push_back (FrameRateCandidate (float (*i) / 2, *i));
		candidates.push_back (FrameRateCandidate (float (*i) * 2, *i));
	}

	/* Pick the best one, bailing early if we hit an exact match */
	float error = numeric_limits<float>::max ();
	boost::optional<FrameRateCandidate> best;
	list<FrameRateCandidate>::iterator i = candidates.begin();
	while (i != candidates.end()) {
		
		if (about_equal (i->source, source_fps)) {
			best = *i;
			break;
		}

		float const e = fabs (i->source - source_fps);
		if (e < error) {
			error = e;
			best = *i;
		}

		++i;
	}

	assert (best);
	return best->dcp;
}

/** @param An arbitrary sampling rate.
 *  @return The appropriate DCP-approved sampling rate (48kHz or 96kHz).
 */
int
dcp_audio_sample_rate (int fs)
{
	if (fs <= 48000) {
		return 48000;
	}

	return 96000;
}

bool operator== (Crop const & a, Crop const & b)
{
	return (a.left == b.left && a.right == b.right && a.top == b.top && a.bottom == b.bottom);
}

bool operator!= (Crop const & a, Crop const & b)
{
	return !(a == b);
}

/** @param index Colour LUT index.
 *  @return Human-readable name.
 */
string
colour_lut_index_to_name (int index)
{
	switch (index) {
	case 0:
		return _("sRGB");
	case 1:
		return _("Rec 709");
	}

	assert (false);
	return N_("");
}

Socket::Socket (int timeout)
	: _deadline (_io_service)
	, _socket (_io_service)
	, _timeout (timeout)
{
	_deadline.expires_at (posix_time::pos_infin);
	check ();
}

void
Socket::check ()
{
	if (_deadline.expires_at() <= asio::deadline_timer::traits_type::now ()) {
		_socket.close ();
		_deadline.expires_at (posix_time::pos_infin);
	}

	_deadline.async_wait (boost::bind (&Socket::check, this));
}

/** Blocking connect.
 *  @param endpoint End-point to connect to.
 */
void
Socket::connect (asio::ip::basic_resolver_entry<asio::ip::tcp> const & endpoint)
{
	_deadline.expires_from_now (posix_time::seconds (_timeout));
	system::error_code ec = asio::error::would_block;
	_socket.async_connect (endpoint, lambda::var(ec) = lambda::_1);
	do {
		_io_service.run_one();
	} while (ec == asio::error::would_block);

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
	_deadline.expires_from_now (posix_time::seconds (_timeout));
	system::error_code ec = asio::error::would_block;

	asio::async_write (_socket, asio::buffer (data, size), lambda::var(ec) = lambda::_1);
	
	do {
		_io_service.run_one ();
	} while (ec == asio::error::would_block);

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
	_deadline.expires_from_now (posix_time::seconds (_timeout));
	system::error_code ec = asio::error::would_block;

	asio::async_read (_socket, asio::buffer (data, size), lambda::var(ec) = lambda::_1);

	do {
		_io_service.run_one ();
	} while (ec == asio::error::would_block);
	
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

/** @param other A Rect.
 *  @return The intersection of this with `other'.
 */
Rect
Rect::intersection (Rect const & other) const
{
	int const tx = max (x, other.x);
	int const ty = max (y, other.y);
	
	return Rect (
		tx, ty,
		min (x + width, other.x + other.width) - tx,
		min (y + height, other.y + other.height) - ty
		);
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

/** Construct an AudioBuffers.  Audio data is undefined after this constructor.
 *  @param channels Number of channels.
 *  @param frames Number of frames to reserve space for.
 */
AudioBuffers::AudioBuffers (int channels, int frames)
	: _channels (channels)
	, _frames (frames)
	, _allocated_frames (frames)
{
	_data = new float*[_channels];
	for (int i = 0; i < _channels; ++i) {
		_data[i] = new float[frames];
	}
}

/** Copy constructor.
 *  @param other Other AudioBuffers; data is copied.
 */
AudioBuffers::AudioBuffers (AudioBuffers const & other)
	: _channels (other._channels)
	, _frames (other._frames)
	, _allocated_frames (other._frames)
{
	_data = new float*[_channels];
	for (int i = 0; i < _channels; ++i) {
		_data[i] = new float[_frames];
		memcpy (_data[i], other._data[i], _frames * sizeof (float));
	}
}

/** AudioBuffers destructor */
AudioBuffers::~AudioBuffers ()
{
	for (int i = 0; i < _channels; ++i) {
		delete[] _data[i];
	}

	delete[] _data;
}

/** @param c Channel index.
 *  @return Buffer for this channel.
 */
float*
AudioBuffers::data (int c) const
{
	assert (c >= 0 && c < _channels);
	return _data[c];
}

/** Set the number of frames that these AudioBuffers will report themselves
 *  as having.
 *  @param f Frames; must be less than or equal to the number of allocated frames.
 */
void
AudioBuffers::set_frames (int f)
{
	assert (f <= _allocated_frames);
	_frames = f;
}

/** Make all samples on all channels silent */
void
AudioBuffers::make_silent ()
{
	for (int i = 0; i < _channels; ++i) {
		make_silent (i);
	}
}

/** Make all samples on a given channel silent.
 *  @param c Channel.
 */
void
AudioBuffers::make_silent (int c)
{
	assert (c >= 0 && c < _channels);
	
	for (int i = 0; i < _frames; ++i) {
		_data[c][i] = 0;
	}
}

/** Copy data from another AudioBuffers to this one.  All channels are copied.
 *  @param from AudioBuffers to copy from; must have the same number of channels as this.
 *  @param frames_to_copy Number of frames to copy.
 *  @param read_offset Offset to read from in `from'.
 *  @param write_offset Offset to write to in `to'.
 */
void
AudioBuffers::copy_from (AudioBuffers* from, int frames_to_copy, int read_offset, int write_offset)
{
	assert (from->channels() == channels());

	assert (from);
	assert (read_offset >= 0 && (read_offset + frames_to_copy) <= from->_allocated_frames);
	assert (write_offset >= 0 && (write_offset + frames_to_copy) <= _allocated_frames);

	for (int i = 0; i < _channels; ++i) {
		memcpy (_data[i] + write_offset, from->_data[i] + read_offset, frames_to_copy * sizeof(float));
	}
}

/** Move audio data around.
 *  @param from Offset to move from.
 *  @param to Offset to move to.
 *  @param frames Number of frames to move.
 */
    
void
AudioBuffers::move (int from, int to, int frames)
{
	if (frames == 0) {
		return;
	}
	
	assert (from >= 0);
	assert (from < _frames);
	assert (to >= 0);
	assert (to < _frames);
	assert (frames > 0);
	assert (frames <= _frames);
	assert ((from + frames) <= _frames);
	assert ((to + frames) <= _frames);
	
	for (int i = 0; i < _channels; ++i) {
		memmove (_data[i] + to, _data[i] + from, frames * sizeof(float));
	}
}

/** Trip an assert if the caller is not in the UI thread */
void
ensure_ui_thread ()
{
	assert (this_thread::get_id() == ui_thread);
}

/** @param v Source video frame.
 *  @param audio_sample_rate Source audio sample rate.
 *  @param frames_per_second Number of video frames per second.
 *  @return Equivalent number of audio frames for `v'.
 */
int64_t
video_frames_to_audio_frames (SourceFrame v, float audio_sample_rate, float frames_per_second)
{
	return ((int64_t) v * audio_sample_rate / frames_per_second);
}

/** @param f Filename.
 *  @return true if this file is a still image, false if it is something else.
 */
bool
still_image_file (string f)
{
	string ext = boost::filesystem::path(f).extension().string();

	transform (ext.begin(), ext.end(), ext.begin(), ::tolower);
	
	return (ext == N_(".tif") || ext == N_(".tiff") || ext == N_(".jpg") || ext == N_(".jpeg") || ext == N_(".png") || ext == N_(".bmp"));
}

/** @return A pair containing CPU model name and the number of processors */
pair<string, int>
cpu_info ()
{
	pair<string, int> info;
	info.second = 0;
	
#ifdef DVDOMATIC_POSIX
	ifstream f (N_("/proc/cpuinfo"));
	while (f.good ()) {
		string l;
		getline (f, l);
		if (boost::algorithm::starts_with (l, N_("model name"))) {
			string::size_type const c = l.find (':');
			if (c != string::npos) {
				info.first = l.substr (c + 2);
			}
		} else if (boost::algorithm::starts_with (l, N_("processor"))) {
			++info.second;
		}
	}
#endif	

	return info;
}

string
audio_channel_name (int c)
{
	assert (MAX_AUDIO_CHANNELS == 6);

	/* TRANSLATORS: these are the names of audio channels; Lfe (sub) is the low-frequency
	   enhancement channel (sub-woofer)./
	*/
	string const channels[] = {
		"Left",
		"Right",
		"Centre",
		"Lfe (sub)",
		"Left surround",
		"Right surround",
	};

	return channels[c];
}

AudioMapping::AudioMapping (int c)
	: _source_channels (c)
{

}

optional<libdcp::Channel>
AudioMapping::source_to_dcp (int c) const
{
	if (c >= _source_channels) {
		return optional<libdcp::Channel> ();
	}

	if (_source_channels == 1) {
		/* mono sources to centre */
		return libdcp::CENTRE;
	}
	
	return static_cast<libdcp::Channel> (c);
}

optional<int>
AudioMapping::dcp_to_source (libdcp::Channel c) const
{
	if (_source_channels == 1) {
		if (c == libdcp::CENTRE) {
			return 0;
		} else {
			return optional<int> ();
		}
	}

	if (static_cast<int> (c) >= _source_channels) {
		return optional<int> ();
	}
	
	return static_cast<int> (c);
}

int
AudioMapping::dcp_channels () const
{
	if (_source_channels == 1) {
		/* The source is mono, so to put the mono channel into
		   the centre we need to generate a 5.1 soundtrack.
		*/
		return 6;
	}

	return _source_channels;
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
}
