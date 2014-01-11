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

#include <boost/algorithm/string.hpp>
#include "subrip.h"
#include "subrip_content.h"
#include "subrip_subtitle.h"
#include "cross.h"
#include "exceptions.h"

#include "i18n.h"

using std::string;
using std::list;
using std::vector;
using boost::shared_ptr;
using boost::lexical_cast;
using boost::algorithm::trim;

SubRip::SubRip (shared_ptr<SubRipContent> content)
{
	FILE* f = fopen_boost (content->path (0), "r");
	if (!f) {
		throw OpenFileError (content->path (0));
	}

	enum {
		COUNTER,
		METADATA,
		CONTENT
	} state = COUNTER;

	char buffer[256];
	int next_count = 1;

	boost::optional<SubRipSubtitle> current;
	list<string> lines;
	
	while (!feof (f)) {
		fgets (buffer, sizeof (buffer), f);
		string line (buffer);
		trim (line);
		
		switch (state) {
		case COUNTER:
		{
			int x = 0;
			try {
				x = lexical_cast<int> (line);
			} catch (...) {

			}
			
			if (x == next_count) {
				state = METADATA;
				++next_count;
				current = SubRipSubtitle ();
			} else {
				throw SubRipError (line, _("a subtitle count"), content->path (0));
			}
		}
		break;
		case METADATA:
		{
			vector<string> p;
			boost::algorithm::split (p, line, boost::algorithm::is_any_of (" "));
			if (p.size() != 3 && p.size() != 7) {
				throw SubRipError (line, _("a time/position line"), content->path (0));
			}

			current->from = convert_time (p[0]);
			current->to = convert_time (p[2]);

			if (p.size() > 3) {
				current->x1 = convert_coordinate (p[3]);
				current->x2 = convert_coordinate (p[4]);
				current->y1 = convert_coordinate (p[5]);
				current->y2 = convert_coordinate (p[6]);
			}
			break;
		}
		case CONTENT:
			if (line.empty ()) {
				state = COUNTER;
				current->pieces = convert_content (lines);
				_subtitles.push_back (current.get ());
				current.reset ();
				lines.clear ();
			} else {
				lines.push_back (line);
			}
			break;
		}
	}

	fclose (f);
}

Time
SubRip::convert_time (string t)
{
	Time r = 0;

	vector<string> a;
	boost::algorithm::split (a, t, boost::is_any_of (":"));
	assert (a.size() == 3);
	r += lexical_cast<int> (a[0]) * 60 * 60 * TIME_HZ;
	r += lexical_cast<int> (a[1]) * 60 * TIME_HZ;

	vector<string> b;
	boost::algorithm::split (b, a[2], boost::is_any_of (","));
	r += lexical_cast<int> (b[0]) * TIME_HZ;
	r += lexical_cast<int> (b[1]) * TIME_HZ / 1000;

	return r;
}

int
SubRip::convert_coordinate (string t)
{
	vector<string> a;
	boost::algorithm::split (a, t, boost::is_any_of (":"));
	assert (a.size() == 2);
	return lexical_cast<int> (a[1]);
}

void
SubRip::maybe_content (list<SubRipSubtitlePiece>& pieces, SubRipSubtitlePiece& p)
{
	if (!p.text.empty ()) {
		pieces.push_back (p);
		p.text.clear ();
	}
}

list<SubRipSubtitlePiece>
SubRip::convert_content (list<string> t)
{
	list<SubRipSubtitlePiece> pieces;
	
	SubRipSubtitlePiece p;

	enum {
		TEXT,
		TAG
	} state = TEXT;

	string tag;

	/* XXX: missing <font> support */
	/* XXX: nesting of tags e.g. <b>foo<i>bar<b>baz</b>fred</i>jim</b> might
	   not work, I think.
	*/

	for (list<string>::const_iterator i = t.begin(); i != t.end(); ++i) {
		for (size_t j = 0; j < i->size(); ++j) {
			switch (state) {
			case TEXT:
				if ((*i)[j] == '<' || (*i)[j] == '{') {
					state = TAG;
				} else {
					p.text += (*i)[j];
				}
				break;
			case TAG:
				if ((*i)[j] == '>' || (*i)[j] == '}') {
					if (tag == "b") {
						maybe_content (pieces, p);
						p.bold = true;
					} else if (tag == "/b") {
						maybe_content (pieces, p);
						p.bold = false;
					} else if (tag == "i") {
						maybe_content (pieces, p);
						p.italic = true;
					} else if (tag == "/i") {
						maybe_content (pieces, p);
						p.italic = false;
					} else if (tag == "u") {
						maybe_content (pieces, p);
						p.underline = true;
					} else if (tag == "/u") {
						maybe_content (pieces, p);
						p.underline = false;
					}
					tag.clear ();
					state = TEXT;
				} else {
					tag += (*i)[j];
				}
				break;
			}
		}
	}

	maybe_content (pieces, p);

	return pieces;
}
