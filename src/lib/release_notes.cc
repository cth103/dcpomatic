/*
    Copyright (C) 2022 Carl Hetherington <cth@carlh.net>

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


#include "config.h"
#include "release_notes.h"
#include "version.h"

#include "i18n.h"


using std::string;
using boost::optional;


optional<string>
find_release_notes(bool dark, optional<string> current)
{
	auto last = Config::instance()->last_release_notes_version();
	if (!current) {
		current = string(dcpomatic_version);
	}
	if (last && *last == *current) {
		return {};
	}

	Config::instance()->set_last_release_notes_version(*current);

	string const colour = dark ? "white" : "black";
	auto const span = String::compose("<span style=\"color: %1\">", colour);

	const string header = String::compose("<h1>%1DCP-o-matic %2 release notes</span></h1>", span, *current);

	if (!last) {
		return header + span +
			_("In this version there are changes to the way that subtitles are positioned.  "
			  "Positioning should now be more correct, with respect to the standards, but you "
			  "should check any subtitles in your project to make sure that they are placed "
			  "where you want them.")
			+ "</span>";
	}

	return {};
}
