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
#include <dcp/interop_subtitle_asset.h>
#include <iostream>

using std::list;
using std::cout;
using boost::shared_ptr;

DCPSubtitleDecoder::DCPSubtitleDecoder (shared_ptr<const DCPSubtitleContent> content)
	: SubtitleDecoder (content)
{
	shared_ptr<dcp::SubtitleAsset> c (load (content->path (0)));
	_subtitles = c->subtitles ();
	_next = _subtitles.begin ();
}

void
DCPSubtitleDecoder::seek (ContentTime time, bool accurate)
{
	SubtitleDecoder::seek (time, accurate);

	_next = _subtitles.begin ();
	list<dcp::SubtitleString>::const_iterator i = _subtitles.begin ();
	while (i != _subtitles.end() && ContentTime::from_seconds (_next->in().as_seconds()) < time) {
		++i;
	}
}

bool
DCPSubtitleDecoder::pass (PassReason)
{
	if (_next == _subtitles.end ()) {
		return true;
	}

	/* Gather all subtitles with the same time period that are next
	   on the list.  We must emit all subtitles for the same time
	   period with the same text_subtitle() call otherwise the
	   SubtitleDecoder will assume there is nothing else at the
	   time of emit the first.
	*/

	list<dcp::SubtitleString> s;
	ContentTimePeriod const p = content_time_period (*_next);

	while (_next != _subtitles.end () && content_time_period (*_next) == p) {
		s.push_back (*_next);
		++_next;
	}

	text_subtitle (p, s);

	return false;
}

list<ContentTimePeriod>
DCPSubtitleDecoder::image_subtitles_during (ContentTimePeriod, bool) const
{
	return list<ContentTimePeriod> ();
}

list<ContentTimePeriod>
DCPSubtitleDecoder::text_subtitles_during (ContentTimePeriod p, bool starting) const
{
	/* XXX: inefficient */

	list<ContentTimePeriod> d;

	for (list<dcp::SubtitleString>::const_iterator i = _subtitles.begin(); i != _subtitles.end(); ++i) {
		ContentTimePeriod period = content_time_period (*i);
		if ((starting && p.contains (period.from)) || (!starting && p.overlaps (period))) {
			d.push_back (period);
		}
	}

	return d;
}

ContentTimePeriod
DCPSubtitleDecoder::content_time_period (dcp::SubtitleString s) const
{
	return ContentTimePeriod (
		ContentTime::from_seconds (s.in().as_seconds ()),
		ContentTime::from_seconds (s.out().as_seconds ())
		);
}
