/*
    Copyright (C) 2014-2018 Carl Hetherington <cth@carlh.net>

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

#include "string_text_file_decoder.h"
#include "string_text_file_content.h"
#include "text_content.h"
#include "text_decoder.h"
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

StringTextFileDecoder::StringTextFileDecoder (shared_ptr<const StringTextFileContent> content, shared_ptr<Log> log)
	: StringTextFile (content)
	, _next (0)
{
	ContentTime first;
	if (!_subtitles.empty()) {
		first = content_time_period(_subtitles[0]).from;
	}
	caption.push_back (shared_ptr<TextDecoder> (new TextDecoder (this, content->only_caption(), log, first)));
}

void
StringTextFileDecoder::seek (ContentTime time, bool accurate)
{
	/* It's worth back-tracking a little here as decoding is cheap and it's nice if we don't miss
	   too many subtitles when seeking.
	*/
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
StringTextFileDecoder::pass ()
{
	if (_next >= _subtitles.size ()) {
		return true;
	}

	ContentTimePeriod const p = content_time_period (_subtitles[_next]);
	only_caption()->emit_plain (p, _subtitles[_next]);

	++_next;
	return false;
}

ContentTimePeriod
StringTextFileDecoder::content_time_period (sub::Subtitle s) const
{
	return ContentTimePeriod (
		ContentTime::from_seconds (s.from.all_as_seconds()),
		ContentTime::from_seconds (s.to.all_as_seconds())
		);
}
