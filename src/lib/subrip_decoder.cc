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

#include <dcp/subtitle_string.h>
#include "subrip_decoder.h"
#include "subrip_content.h"

using std::list;
using std::vector;
using boost::shared_ptr;

SubRipDecoder::SubRipDecoder (shared_ptr<const SubRipContent> content)
	: SubtitleDecoder (content)
	, SubRip (content)
	, _next (0)
{

}

void
SubRipDecoder::seek (ContentTime time, bool accurate)
{
	SubtitleDecoder::seek (time, accurate);
	
	_next = 0;
	list<SubRipSubtitlePiece>::const_iterator i = _subtitles[_next].pieces.begin();
	while (i != _subtitles[_next].pieces.end() && _subtitles[_next].period.from < time) {
		++i;
	}
	
}

bool
SubRipDecoder::pass ()
{
	if (_next >= _subtitles.size ()) {
		return true;
	}
	
	list<dcp::SubtitleString> out;
	for (list<SubRipSubtitlePiece>::const_iterator i = _subtitles[_next].pieces.begin(); i != _subtitles[_next].pieces.end(); ++i) {
		out.push_back (
			dcp::SubtitleString (
				"Arial",
				i->italic,
				dcp::Color (255, 255, 255),
				72,
				dcp::Time (rint (_subtitles[_next].period.from.seconds() * 250)),
				dcp::Time (rint (_subtitles[_next].period.to.seconds() * 250)),
				0.9,
				dcp::BOTTOM,
				i->text,
				dcp::NONE,
				dcp::Color (255, 255, 255),
				0,
				0
				)
			);
	}

	text_subtitle (out);
	_next++;
	return false;
}

bool
SubRipDecoder::has_subtitle_during (ContentTimePeriod p) const
{
	/* XXX: inefficient */

	for (vector<SubRipSubtitle>::const_iterator i = _subtitles.begin(); i != _subtitles.end(); ++i) {
		if (p.overlaps (i->period)) {
			return true;
		}
	}

	return false;
}
