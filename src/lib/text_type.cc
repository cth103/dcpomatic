/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#include "compose.hpp"
#include "dcpomatic_assert.h"
#include "exceptions.h"
#include "text_type.h"
#include "types.h"

#include "i18n.h"


using std::string;


TextType
string_to_text_type(string s)
{
	if (s == "unknown") {
		return TextType::UNKNOWN;
	} else if (s == "open-subtitle") {
		return TextType::OPEN_SUBTITLE;
	} else if (s == "closed-caption") {
		return TextType::CLOSED_CAPTION;
	} else {
		throw MetadataError(String::compose("Unknown text type %1", s));
	}
}

string
text_type_to_string(TextType t)
{
	switch (t) {
	case TextType::UNKNOWN:
		return "unknown";
	case TextType::OPEN_SUBTITLE:
		return "open-subtitle";
	case TextType::CLOSED_CAPTION:
		return "closed-caption";
	default:
		DCPOMATIC_ASSERT(false);
	}
}

string
text_type_to_name(TextType t)
{
	switch (t) {
	case TextType::UNKNOWN:
		return _("Timed text");
	case TextType::OPEN_SUBTITLE:
		return _("Open subtitles");
	case TextType::CLOSED_CAPTION:
		return _("Closed captions");
	default:
		DCPOMATIC_ASSERT(false);
	}
}


