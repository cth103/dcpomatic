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

#ifndef DCPOMATIC_CONTENT_TEXT_H
#define DCPOMATIC_CONTENT_TEXT_H

#include "dcpomatic_time.h"
#include "rect.h"
#include "types.h"
#include "bitmap_text.h"
#include <dcp/subtitle_string.h>
#include <list>

class Image;

class ContentText
{
public:
	explicit ContentText (ContentTime f, TextType t)
		: _from (f)
		, _type (t)
	{}

	ContentTime from () const {
		return _from;
	}

	TextType type () const {
		return _type;
	}

private:
	ContentTime _from;
	TextType _type;
};

class ContentBitmapCaption : public ContentText
{
public:
	ContentBitmapCaption (ContentTime f, TextType type, boost::shared_ptr<Image> im, dcpomatic::Rect<double> r)
		: ContentText (f, type)
		, sub (im, r)
	{}

	/* Our text, with its rectangle unmodified by any offsets or scales that the content specifies */
	BitmapText sub;
};

/** A text caption.  We store the time period separately (as well as in the dcp::SubtitleStrings)
 *  as the dcp::SubtitleString timings are sometimes quite heavily quantised and this causes problems
 *  when we want to compare the quantised periods to the unquantised ones.
 */
class ContentTextCaption : public ContentText
{
public:
	ContentTextCaption (ContentTime f, TextType type, std::list<dcp::SubtitleString> s)
		: ContentText (f, type)
		, subs (s)
	{}

	std::list<dcp::SubtitleString> subs;
};

#endif
