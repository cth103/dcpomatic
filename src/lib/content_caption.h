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
#include "bitmap_caption.h"
#include <dcp/subtitle_string.h>
#include <list>

class Image;

class ContentCaption
{
public:
	explicit ContentCaption (ContentTime f, CaptionType t)
		: _from (f)
		, _type (t)
	{}

	ContentTime from () const {
		return _from;
	}

	CaptionType type () const {
		return _type;
	}

private:
	ContentTime _from;
	CaptionType _type;
};

class ContentBitmapCaption : public ContentCaption
{
public:
	ContentBitmapCaption (ContentTime f, CaptionType type, boost::shared_ptr<Image> im, dcpomatic::Rect<double> r)
		: ContentCaption (f, type)
		, sub (im, r)
	{}

	/* Our text, with its rectangle unmodified by any offsets or scales that the content specifies */
	BitmapCaption sub;
};

/** A text caption.  We store the time period separately (as well as in the dcp::SubtitleStrings)
 *  as the dcp::SubtitleString timings are sometimes quite heavily quantised and this causes problems
 *  when we want to compare the quantised periods to the unquantised ones.
 */
class ContentTextCaption : public ContentCaption
{
public:
	ContentTextCaption (ContentTime f, CaptionType type, std::list<dcp::SubtitleString> s)
		: ContentCaption (f, type)
		, subs (s)
	{}

	std::list<dcp::SubtitleString> subs;
};

#endif
