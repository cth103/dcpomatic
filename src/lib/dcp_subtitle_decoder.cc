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

#include "dcp_subtitle_decoder.h"
#include "dcp_subtitle_content.h"
#include <dcp/interop_subtitle_content.h>

using std::list;
using std::cout;
using boost::shared_ptr;

DCPSubtitleDecoder::DCPSubtitleDecoder (shared_ptr<const DCPSubtitleContent> content)
	: SubtitleDecoder (content)
{
	shared_ptr<dcp::SubtitleContent> c (load (content->path (0)));
	_subtitles = c->subtitles ();
	_next = _subtitles.begin ();
}

void
DCPSubtitleDecoder::seek (ContentTime time, bool accurate)
{
	SubtitleDecoder::seek (time, accurate);

	_next = _subtitles.begin ();
	list<dcp::SubtitleString>::const_iterator i = _subtitles.begin ();
	while (i != _subtitles.end() && ContentTime::from_seconds (_next->in().to_seconds()) < time) {
		++i;
	}
}

bool
DCPSubtitleDecoder::pass ()
{
	if (_next == _subtitles.end ()) {
		return true;
	}

	list<dcp::SubtitleString> s;
	s.push_back (*_next);
	text_subtitle (s);
	++_next;

	return false;
}

list<ContentTimePeriod>
DCPSubtitleDecoder::subtitles_during (ContentTimePeriod p, bool starting) const
{
	/* XXX: inefficient */

	list<ContentTimePeriod> d;

	for (list<dcp::SubtitleString>::const_iterator i = _subtitles.begin(); i != _subtitles.end(); ++i) {
		ContentTimePeriod period (
			ContentTime::from_seconds (i->in().to_seconds ()),
			ContentTime::from_seconds (i->out().to_seconds ())
			);
		
		if ((starting && p.contains (period.from)) || (!starting && p.overlaps (period))) {
			d.push_back (period);
		}
	}

	return d;
}

