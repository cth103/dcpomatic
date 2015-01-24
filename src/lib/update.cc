/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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

#include <string>
#include <boost/algorithm/string.hpp>
#include <curl/curl.h>
#include <libcxml/cxml.h>
#include <dcp/raw_convert.h>
#include "update.h"
#include "version.h"
#include "ui_signaller.h"
#include "safe_stringstream.h"
#include "config.h"
#include "util.h"

#define BUFFER_SIZE 1024

using std::cout;
using std::min;
using std::string;
using std::vector;
using dcp::raw_convert;
using boost::is_any_of;
using boost::ends_with;

/** Singleton instance */
UpdateChecker* UpdateChecker::_instance = 0;

static size_t
write_callback_wrapper (void* data, size_t size, size_t nmemb, void* user)
{
	return reinterpret_cast<UpdateChecker*>(user)->write_callback (data, size, nmemb);
}

/** Construct an UpdateChecker.  This sets things up and starts a thread to
 *  do the work.
 */
UpdateChecker::UpdateChecker ()
	: _buffer (new char[BUFFER_SIZE])
	, _offset (0)
	, _curl (0)
	, _state (NOT_RUN)
	, _emits (0)
	, _to_do (0)
{
	curl_global_init (CURL_GLOBAL_ALL);
	_curl = curl_easy_init ();

	curl_easy_setopt (_curl, CURLOPT_URL, "http://dcpomatic.com/update");
	curl_easy_setopt (_curl, CURLOPT_WRITEFUNCTION, write_callback_wrapper);
	curl_easy_setopt (_curl, CURLOPT_WRITEDATA, this);
	curl_easy_setopt (_curl, CURLOPT_TIMEOUT, 20);
	
	string const agent = "dcpomatic/" + string (dcpomatic_version);
	curl_easy_setopt (_curl, CURLOPT_USERAGENT, agent.c_str ());

	_thread = new boost::thread (boost::bind (&UpdateChecker::thread, this));
}

UpdateChecker::~UpdateChecker ()
{
	/* We are not cleaning up our thread, but hey well */
	
	curl_easy_cleanup (_curl);
	curl_global_cleanup ();
	delete[] _buffer;
}

/** Start running the update check */
void
UpdateChecker::run ()
{
	boost::mutex::scoped_lock lm (_process_mutex);
	_to_do++;
	_condition.notify_one ();
}

void
UpdateChecker::thread ()
{
	while (true) {
		/* Block until there is something to do */
		boost::mutex::scoped_lock lock (_process_mutex);
		while (_to_do == 0) {
			_condition.wait (lock);
		}
		--_to_do;
		lock.unlock ();
		
		try {
			_offset = 0;

			/* Perform the request */
			
			int r = curl_easy_perform (_curl);
			if (r != CURLE_OK) {
				set_state (FAILED);
				return;
			}

			/* Parse the reply */
			
			_buffer[_offset] = '\0';
			string s (_buffer);
			cxml::Document doc ("Update");
			doc.read_string (s);

			/* Read the current stable and test version numbers */

			string stable;
			string test;

			{
				boost::mutex::scoped_lock lm (_data_mutex);
				stable = doc.string_child ("Stable");
				test = doc.string_child ("Test");
			}

			if (version_less_than (dcpomatic_version, stable)) {
				_stable = stable;
			}
			
			if (Config::instance()->check_for_test_updates() && version_less_than (dcpomatic_version, test)) {
				_test = test;
			}

			if (_stable || _test) {
				set_state (YES);
			} else {
				set_state (NO);
			}
		} catch (...) {
			set_state (FAILED);
		}
	}
}
	
size_t
UpdateChecker::write_callback (void* data, size_t size, size_t nmemb)
{
	size_t const t = min (size * nmemb, size_t (BUFFER_SIZE - _offset - 1));
	memcpy (_buffer + _offset, data, t);
	_offset += t;
	return t;
}

void
UpdateChecker::set_state (State s)
{
	{
		boost::mutex::scoped_lock lm (_data_mutex);
		_state = s;
		_emits++;
	}

	ui_signaller->emit (boost::bind (boost::ref (StateChanged)));
}

UpdateChecker *
UpdateChecker::instance ()
{
	if (!_instance) {
		_instance = new UpdateChecker ();
	}

	return _instance;
}

bool
UpdateChecker::version_less_than (string const & a, string const & b)
{
	vector<string> ap;
	split (ap, a, is_any_of ("."));
	vector<string> bp;
	split (bp, b, is_any_of ("."));

	DCPOMATIC_ASSERT (ap.size() == 3 && bp.size() == 3);

	if (ap[0] != bp[0]) {
		return raw_convert<int> (ap[0]) < raw_convert<int> (bp[0]);
	}

	if (ap[1] != bp[1]) {
		return raw_convert<int> (ap[1]) < raw_convert<int> (bp[1]);
	}
	float am;
	if (ends_with (ap[2], "devel")) {
		am = raw_convert<int> (ap[2].substr (0, ap[2].length() - 5)) + 0.5;
	} else {
		am = raw_convert<int> (ap[2]);
	}
	
	float bm;
	if (ends_with (bp[2], "devel")) {
		bm = raw_convert<int> (bp[2].substr (0, bp[2].length() - 5)) + 0.5;
	} else {
		bm = raw_convert<int> (bp[2]);
	}
	
	return am < bm;
}
