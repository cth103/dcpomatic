/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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


#include "i18n_setup.h"
#include "variant.h"
#include <fmt/format.h>
#include <boost/filesystem.hpp>
#include <libintl.h>
#include <CoreFoundation/CoreFoundation.h>


using std::string;


boost::filesystem::path
dcpomatic::mo_path()
{
	return variant::dcpomatic_app() + "/Contents/Resources";
}


void
dcpomatic::setup_i18n(string forced_language)
{
	if (!forced_language.empty()) {
		/* Override our environment forced_languageuage.  Note that the caller must not
		   free the string passed into putenv().
		*/
		string s = fmt::format("LANGUAGE={}", forced_language);
		putenv(strdup(s.c_str()));
		s = fmt::format("LANG={}", forced_language);
		putenv(strdup(s.c_str()));
	}

	auto get_locale_value = [](CFLocaleKey key) {
		CFLocaleRef cflocale = CFLocaleCopyCurrent();
		auto value = (CFStringRef) CFLocaleGetValue(cflocale, key);
		char buffer[64];
		CFStringGetCString(value, buffer, sizeof(buffer), kCFStringEncodingUTF8);
		CFRelease(cflocale);
		return string(buffer);
	};

	/* We want to keep using the user's macOS-configured locale, partly because this feels
	 * like the right thing to do but mostly because that's what the wxWidgets side will do,
	 * and we must agree.
	 */
	auto lc = fmt::format("LC_ALL={}_{}", get_locale_value(kCFLocaleLanguageCode), get_locale_value(kCFLocaleCountryCode));
	putenv(strdup(lc.c_str()));

	setlocale(LC_ALL, "");

	textdomain("libdcpomatic2");

	bindtextdomain("libdcpomatic2", mo_path().string().c_str());
	bind_textdomain_codeset("libdcpomatic2", "UTF8");
}

