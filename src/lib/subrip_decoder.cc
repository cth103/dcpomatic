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
using std::string;
using boost::shared_ptr;
using boost::optional;

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
	while (_next < _subtitles.size() && ContentTime::from_seconds (_subtitles[_next].from.metric().get().all_as_seconds ()) < time) {
		++_next;
	}
}

bool
SubRipDecoder::pass ()
{
	if (_next >= _subtitles.size ()) {
		return true;
	}

	list<dcp::SubtitleString> out;
	for (list<sub::Line>::const_iterator i = _subtitles[_next].lines.begin(); i != _subtitles[_next].lines.end(); ++i) {
		for (list<sub::Block>::const_iterator j = i->blocks.begin(); j != i->blocks.end(); ++j) {

			dcp::VAlign va = dcp::TOP;
			if (i->vertical_position.reference) {
				switch (i->vertical_position.reference.get ()) {
				case sub::TOP_OF_SCREEN:
					va = dcp::TOP;
					break;
				case sub::CENTRE_OF_SCREEN:
					va = dcp::CENTER;
					break;
				case sub::BOTTOM_OF_SCREEN:
					va = dcp::BOTTOM;
				}
			}
			
			out.push_back (
				dcp::SubtitleString (
					SubRipContent::font_id,
					j->italic,
					dcp::Color (255, 255, 255),
					j->font_size,
					dcp::Time (rint (_subtitles[_next].from.metric().get().all_as_milliseconds() / 4)),
					dcp::Time (rint (_subtitles[_next].to.metric().get().all_as_milliseconds() / 4)),
					i->vertical_position.line.get() * (1.5 / 22) + 0.8,
					va,
					j->text,
					dcp::NONE,
					dcp::Color (255, 255, 255),
					0,
					0
					)
				);
		}
	}

	text_subtitle (out);
	++_next;
	return false;
}

list<ContentTimePeriod>
SubRipDecoder::subtitles_during (ContentTimePeriod p, bool starting) const
{
	/* XXX: inefficient */

	list<ContentTimePeriod> d;

	for (vector<sub::Subtitle>::const_iterator i = _subtitles.begin(); i != _subtitles.end(); ++i) {

		ContentTimePeriod t (
			ContentTime::from_seconds (i->from.metric().get().all_as_seconds()),
			ContentTime::from_seconds (i->to.metric().get().all_as_seconds())
			);
		
		if ((starting && p.contains (t.from)) || (!starting && p.overlaps (t))) {
			d.push_back (t);
		}
	}

	return d;
}
