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


#ifndef DCPOMATIC_TEXT_TYPE_H
#define DCPOMATIC_TEXT_TYPE_H


#include <string>


/** Type of captions.
 *
 *  The generally accepted definitions seem to be:
 *  - subtitles: text for an audience who doesn't speak the film's language
 *  - captions:  text for a hearing-impaired audience
 *  - open:      on-screen
 *  - closed:    only visible by some audience members
 *
 *  There is some use of the word `subtitle' in the code which may mean
 *  caption in some contexts.
 */
enum class TextType
{
	UNKNOWN,
	OPEN_SUBTITLE,
	OPEN_CAPTION,
	CLOSED_SUBTITLE,
	CLOSED_CAPTION,
	COUNT
};

extern std::string text_type_to_string(TextType t);
extern std::string text_type_to_name(TextType t);
extern TextType string_to_text_type(std::string s);


#endif

