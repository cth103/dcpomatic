/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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


#include "dcp_text_track.h"
#include "compose.hpp"
#include <fmt/format.h>
#include <string>

#include "i18n.h"


using std::string;
using boost::optional;


DCPTextTrack::DCPTextTrack (cxml::ConstNodePtr node)
{
	name = node->string_child("Name");
	if (auto lang = node->optional_string_child("Language")) {
		language = dcp::LanguageTag(*lang);
	}
}


DCPTextTrack::DCPTextTrack (string name_, optional<dcp::LanguageTag> language_)
	: name (name_)
	, language (language_)
{

}


string
DCPTextTrack::summary () const
{
	return fmt::format("{} ({})", name, language ? language->as_string() : _("Unknown"));
}

void
DCPTextTrack::as_xml (xmlpp::Element* parent) const
{
	cxml::add_text_child(parent, "Name", name);
	if (language) {
		cxml::add_text_child(parent, "Language", language->as_string());
	}
}


bool
operator== (DCPTextTrack const & a, DCPTextTrack const & b)
{
	return a.name == b.name && a.language == b.language;
}


bool
operator!= (DCPTextTrack const & a, DCPTextTrack const & b)
{
	return !(a == b);
}


bool
operator< (DCPTextTrack const & a, DCPTextTrack const & b)
{
	if (a.name != b.name) {
		return a.name < b.name;
	}

	return a.language < b.language;
}
