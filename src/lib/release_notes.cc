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
find_release_notes(optional<string> current)
{
	auto last = Config::instance()->last_release_notes_version();
	if (!current) {
		current = string(dcpomatic_version);
	}
	if (last && *last == *current) {
		return {};
	}

	Config::instance()->set_last_release_notes_version(*current);

	const string header = String::compose("<h1>DCP-o-matic %1 release notes</h1>", *current);

	if (!last) {
		return header +
			_("In this version there are changes to the way that subtitles are positioned.  "
			  "Positioning should now be more correct, with respect to the standards, but you "
			  "should check any subtitles in your project to make sure that they are placed "
			  "where you want them.");
	}

	return {};
}
