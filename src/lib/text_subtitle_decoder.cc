/*
    Copyright (C) 2014-2016 Carl Hetherington <cth@carlh.net>

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

#include "text_subtitle_decoder.h"
#include "text_subtitle_content.h"
#include "subtitle_content.h"
#include <dcp/subtitle_string.h>
#include <boost/foreach.hpp>
#include <iostream>

using std::list;
using std::vector;
using std::string;
using std::cout;
using std::max;
using boost::shared_ptr;
using boost::optional;
using boost::dynamic_pointer_cast;

TextSubtitleDecoder::TextSubtitleDecoder (shared_ptr<const TextSubtitleContent> content, shared_ptr<Log> log)
	: TextSubtitle (content)
	, _next (0)
{
	subtitle.reset (new SubtitleDecoder (this, content->subtitle, log));
}

void
TextSubtitleDecoder::seek (ContentTime time, bool accurate)
{
	time -= ContentTime::from_seconds (5);
	if (time < ContentTime()) {
		time = ContentTime();
	}

	Decoder::seek (time, accurate);

	_next = 0;
	while (_next < _subtitles.size() && ContentTime::from_seconds (_subtitles[_next].from.all_as_seconds ()) < time) {
		++_next;
	}
}

bool
TextSubtitleDecoder::pass ()
{
	if (_next >= _subtitles.size ()) {
		return true;
	}

	ContentTimePeriod const p = content_time_period (_subtitles[_next]);
	subtitle->emit_text (p, _subtitles[_next]);

	++_next;
	return false;
}

ContentTimePeriod
TextSubtitleDecoder::content_time_period (sub::Subtitle s) const
{
	return ContentTimePeriod (
		ContentTime::from_seconds (s.from.all_as_seconds()),
		ContentTime::from_seconds (s.to.all_as_seconds())
		);
}
