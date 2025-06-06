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


#define UNICODE 1


#include "i18n_setup.h"
#include <fmt/format.h>
#include <boost/filesystem.hpp>
#include <windows.h>
#include <libintl.h>


using std::string;


boost::filesystem::path
dcpomatic::mo_path()
{
	wchar_t buffer[512];
	GetModuleFileName(0, buffer, 512 * sizeof(wchar_t));
	boost::filesystem::path p(buffer);
	p = p.parent_path();
	p = p.parent_path();
	p /= "locale";
	return p;
}


void
dcpomatic::setup_i18n(string forced_language)
{
	if (!forced_language.empty()) {
		/* Override our environment language.  Note that the caller must not
		   free the string passed into putenv().
		*/
		auto s = fmt::format("LANGUAGE={}", forced_language);
		putenv(strdup(s.c_str()));
		s = fmt::format("LANG={}", forced_language);
		putenv(strdup(s.c_str()));
		s = fmt::format("LC_ALL={}", forced_language);
		putenv(strdup(s.c_str()));
	}

	setlocale(LC_ALL, "");
	textdomain("libdcpomatic2");

	bindtextdomain("libdcpomatic2", mo_path().string().c_str());
	bind_textdomain_codeset("libdcpomatic2", "UTF8");
}

