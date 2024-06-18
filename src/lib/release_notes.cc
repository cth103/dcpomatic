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
#include "variant.h"
#include "version.h"
#include <dcp/raw_convert.h>
#include <boost/algorithm/string.hpp>

#include "i18n.h"


using std::string;
using std::vector;
using boost::optional;


bool
before(optional<string> last, string current)
{
	if (!last) {
		return true;
	}

	vector<string> last_parts;
	boost::split(last_parts, *last, boost::is_any_of("."));
	vector<string> current_parts;
	boost::split(current_parts, current, boost::is_any_of("."));

	if (last_parts.size() != 3 || current_parts.size() != 3) {
		/* One or other is a git version; don't bother reporting anything */
		return false;
	}

	for (int i = 0; i < 3; ++i) {
		if (dcp::raw_convert<int>(last_parts[i]) < dcp::raw_convert<int>(current_parts[i])) {
		    return true;
		}
	}

	return false;
}


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

	vector<string> notes;
	if (!last) {
		notes.push_back(_("In this version there are changes to the way that subtitles are positioned. "
				  "Positioning should now be more correct, with respect to the standards, but you "
				  "should check any subtitles in your project to make sure that they are placed "
				  "where you want them."));
	}

	if (before(last, "2.17.19")) {
		notes.push_back(_("The vertical offset control for some subtitles now works in the opposite direction "
				  "to how it was before.   You should check any subtitles in your project to make sure "
				  "that they are placed where you want them."));
	}

	if (notes.empty()) {
		return {};
	}

	string const colour = dark ? "white" : "black";
	auto const span = String::compose("<span style=\"color: %1\">", colour);

	auto output = String::compose("<h1>%1%2 %3 release notes</span></h1><ul>", span, variant::dcpomatic(), *current);

	for (auto const& note: notes) {
		output += string("<li>") + span + note + "</span>";
	}

	output += "</ul>";

	return output;
}
