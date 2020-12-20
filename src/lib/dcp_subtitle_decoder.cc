/*
    Copyright (C) 2014-2020 Carl Hetherington <cth@carlh.net>

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

#include "dcp_subtitle_decoder.h"
#include "dcp_subtitle_content.h"
#include <dcp/interop_subtitle_asset.h>
#include <dcp/load_font_node.h>
#include <iostream>

using std::cout;
using std::list;
using std::map;
using std::string;
using std::vector;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using boost::bind;
using namespace dcpomatic;

DCPSubtitleDecoder::DCPSubtitleDecoder (shared_ptr<const Film> film, shared_ptr<const DCPSubtitleContent> content)
	: Decoder (film)
{
	shared_ptr<dcp::SubtitleAsset> c (load (content->path (0)));
	c->fix_empty_font_ids ();
	_subtitles = c->subtitles ();
	_next = _subtitles.begin ();

	ContentTime first;
	if (_next != _subtitles.end()) {
		first = content_time_period(*_next).from;
	}
	text.push_back (shared_ptr<TextDecoder> (new TextDecoder (this, content->only_text(), first)));

	map<string, dcp::ArrayData> fm = c->font_data();
	for (map<string, dcp::ArrayData>::const_iterator j = fm.begin(); j != fm.end(); ++j) {
		_fonts.push_back (FontData(j->first, j->second));
	}

	/* Add a default font for any LoadFont nodes in our file which we haven't yet found fonts for */
	BOOST_FOREACH (shared_ptr<dcp::LoadFontNode> i, c->load_font_nodes()) {
		if (fm.find(i->id) == fm.end()) {
			_fonts.push_back (FontData(i->id, dcp::ArrayData(default_font_file())));
		}
	}
}

void
DCPSubtitleDecoder::seek (ContentTime time, bool accurate)
{
	Decoder::seek (time, accurate);

	_next = _subtitles.begin ();
	list<shared_ptr<dcp::Subtitle> >::const_iterator i = _subtitles.begin ();
	while (i != _subtitles.end() && ContentTime::from_seconds ((*_next)->in().as_seconds()) < time) {
		++i;
	}
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

	list<dcp::SubtitleString> s;
	list<dcp::SubtitleImage> i;
	ContentTimePeriod const p = content_time_period (*_next);

	while (_next != _subtitles.end () && content_time_period (*_next) == p) {
		shared_ptr<dcp::SubtitleString> ns = dynamic_pointer_cast<dcp::SubtitleString>(*_next);
		if (ns) {
			s.push_back (*ns);
			++_next;
		} else {
			/* XXX: perhaps these image subs should also be collected together like the string ones are;
			   this would need to be done both here and in DCPDecoder.
			*/

			shared_ptr<dcp::SubtitleImage> ni = dynamic_pointer_cast<dcp::SubtitleImage>(*_next);
			if (ni) {
				emit_subtitle_image (p, *ni, film()->frame_size(), only_text());
				++_next;
			}
		}
	}

	only_text()->emit_plain (p, s);
	return false;
}

ContentTimePeriod
DCPSubtitleDecoder::content_time_period (shared_ptr<dcp::Subtitle> s) const
{
	return ContentTimePeriod (
		ContentTime::from_seconds (s->in().as_seconds ()),
		ContentTime::from_seconds (s->out().as_seconds ())
		);
}


vector<dcpomatic::FontData>
DCPSubtitleDecoder::fonts () const
{
	return _fonts;
}

