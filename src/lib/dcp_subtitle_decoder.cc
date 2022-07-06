/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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


#include "dcp_subtitle_content.h"
#include "dcp_subtitle_decoder.h"
#include "font.h"
#include "text_content.h"
#include <dcp/interop_subtitle_asset.h>
#include <dcp/load_font_node.h>


using std::dynamic_pointer_cast;
using std::list;
using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;
using boost::optional;
using namespace dcpomatic;


DCPSubtitleDecoder::DCPSubtitleDecoder (shared_ptr<const Film> film, shared_ptr<const DCPSubtitleContent> content)
	: Decoder (film)
{
	/* Load the XML or MXF file */
	auto const asset = load (content->path(0));
	asset->fix_empty_font_ids ();
	_subtitles = asset->subtitles ();
	_next = _subtitles.begin ();

	text.push_back (make_shared<TextDecoder>(this, content->only_text()));
	update_position();
}


void
DCPSubtitleDecoder::seek (ContentTime time, bool accurate)
{
	Decoder::seek (time, accurate);

	_next = _subtitles.begin ();
	auto i = _subtitles.begin ();
	while (i != _subtitles.end() && ContentTime::from_seconds ((*_next)->in().as_seconds()) < time) {
		++i;
	}

	update_position();
}


bool
DCPSubtitleDecoder::pass ()
{
	if (_next == _subtitles.end ()) {
		return true;
	}

	/* Gather all subtitles with the same time period that are next
	   on the list.  We must emit all subtitles for the same time
	   period with the same emit*() call otherwise the
	   TextDecoder will assume there is nothing else at the
	   time of emitting the first.
	*/

	vector<dcp::SubtitleString> s;
	vector<dcp::SubtitleImage> i;
	auto const p = content_time_period (*_next);

	while (_next != _subtitles.end () && content_time_period (*_next) == p) {
		auto ns = dynamic_pointer_cast<const dcp::SubtitleString>(*_next);
		if (ns) {
			s.push_back (*ns);
			++_next;
		} else {
			/* XXX: perhaps these image subs should also be collected together like the string ones are;
			   this would need to be done both here and in DCPDecoder.
			*/

			auto ni = dynamic_pointer_cast<const dcp::SubtitleImage>(*_next);
			if (ni) {
				emit_subtitle_image (p, *ni, film()->frame_size(), only_text());
				++_next;
			}
		}
	}

	only_text()->emit_plain (p, s);

	update_position();

	return false;
}


ContentTimePeriod
DCPSubtitleDecoder::content_time_period (shared_ptr<const dcp::Subtitle> s) const
{
	return {
		ContentTime::from_seconds(s->in().as_seconds()),
		ContentTime::from_seconds(s->out().as_seconds())
	};
}


/** @return time of first subtitle, if there is one */
optional<ContentTime>
DCPSubtitleDecoder::first () const
{
	if (_subtitles.empty()) {
		return {};
	}

	return ContentTime::from_seconds(_subtitles[0]->in().as_seconds());
}


void
DCPSubtitleDecoder::update_position()
{
	if (_next != _subtitles.end()) {
		only_text()->maybe_set_position(
			ContentTime::from_seconds((*_next)->in().as_seconds())
			);
	}
}

