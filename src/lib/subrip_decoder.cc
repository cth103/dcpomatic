/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include "subrip_decoder.h"

using std::list;
using boost::shared_ptr;

SubRipDecoder::SubRipDecoder (shared_ptr<const Film> film, shared_ptr<const SubRipContent> content)
	: Decoder (film)
	, SubtitleDecoder (film)
	, SubRip (content)
	, _next (0)
{

}

bool
SubRipDecoder::pass ()
{
	if (_next >= _subtitles.size ()) {
		return true;
	}
	
	list<libdcp::Subtitle> out;
	for (list<SubRipSubtitlePiece>::const_iterator i = _subtitles[_next].pieces.begin(); i != _subtitles[_next].pieces.end(); ++i) {
		out.push_back (
			libdcp::Subtitle (
				"Arial",
				i->italic,
				libdcp::Color (255, 255, 255),
				72,
				_subtitles[_next].from,
				_subtitles[_next].to,
				0.9,
				libdcp::BOTTOM,
				i->text,
				libdcp::NONE,
				libdcp::Color (255, 255, 255),
				0,
				0
				)
			);
	}

	text_subtitle (out);
	_next++;
	return false;
}
