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

/** @file src/util.h
 *  @brief Some utility functions and classes.
 */

#ifndef DCPOMATIC_UTIL_H
#define DCPOMATIC_UTIL_H

#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/optional.hpp>
#include <boost/filesystem.hpp>
#include <dcp/util.h>
#include <dcp/signer.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
}
#include "compose.hpp"
#include "types.h"
#include "video_content.h"

#ifdef DCPOMATIC_DEBUG
#define TIMING(...) _film->log()->microsecond_log (String::compose (__VA_ARGS__), Log::TIMING);
#else
#define TIMING(...)
#endif

#undef check

/** The maximum number of audio channels that we can cope with */
#define MAX_AUDIO_CHANNELS 8

#define DCPOMATIC_HELLO "Boys, you gotta learn not to talk to nuns that way"

namespace libdcp {
	class Signer;
}

class Job;

extern std::string seconds_to_hms (int);
extern std::string seconds_to_approximate_hms (int);
extern void stacktrace (std::ostream &, int);
extern std::string dependency_version_summary ();
extern double seconds (struct timeval);
extern void dcpomatic_setup ();
extern void dcpomatic_setup_gettext_i18n (std::string);
extern std::vector<std::string> split_at_spaces_considering_quotes (std::string);
extern std::string md5_digest (std::vector<boost::filesystem::path>, boost::shared_ptr<Job>);
extern std::string md5_digest (void const *, int);
extern void ensure_ui_thread ();
extern std::string audio_channel_name (int);
extern bool valid_image_file (boost::filesystem::path);
#ifdef DCPOMATIC_WINDOWS
extern boost::filesystem::path mo_path ();
#endif
extern std::string tidy_for_filename (std::string);
extern boost::shared_ptr<const dcp::Signer> make_signer ();
extern dcp::Size fit_ratio_within (float ratio, dcp::Size);
extern std::string entities_to_text (std::string e);
extern std::map<std::string, std::string> split_get_request (std::string url);
extern int dcp_audio_frame_rate (int);
extern int stride_round_up (int, int const *, int);
extern std::multimap<std::string, std::string> read_key_value (std::istream& s);
extern int get_required_int (std::multimap<std::string, std::string> const & kv, std::string k);
extern float get_required_float (std::multimap<std::string, std::string> const & kv, std::string k);
extern std::string get_required_string (std::multimap<std::string, std::string> const & kv, std::string k);
extern int get_optional_int (std::multimap<std::string, std::string> const & kv, std::string k);
extern std::string get_optional_string (std::multimap<std::string, std::string> const & kv, std::string k);
extern void* wrapped_av_malloc (size_t);
extern int64_t divide_with_round (int64_t a, int64_t b);

/** @class Socket
 *  @brief A class to wrap a boost::asio::ip::tcp::socket with some things
 *  that are useful for DCP-o-matic.
 *
 *  This class wraps some things that I could not work out how to do with boost;
 *  most notably, sync read/write calls with timeouts.
 */
class Socket
{
public:
	Socket (int timeout = 30);
	~Socket ();

	/** @return Our underlying socket */
	boost::asio::ip::tcp::socket& socket () {
		return _socket;
	}

	void connect (boost::asio::ip::tcp::endpoint);
	void accept (int);

	void write (uint32_t n);
	void write (uint8_t const * data, int size);
	
	void read (uint8_t* data, int size);
	uint32_t read_uint32 ();
	
private:
	void check ();

	Socket (Socket const &);

	boost::asio::io_service _io_service;
	boost::asio::deadline_timer _deadline;
	boost::asio::ip::tcp::socket _socket;
	boost::asio::ip::tcp::acceptor* _acceptor;
	int _timeout;
};

class LocaleGuard
{
public:
	LocaleGuard ();
	~LocaleGuard ();
	
private:
	char* _old;
};


#endif

