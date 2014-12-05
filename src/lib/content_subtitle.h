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

#ifndef DCPOMATIC_CONTENT_SUBTITLE_H
#define DCPOMATIC_CONTENT_SUBTITLE_H

#include "dcpomatic_time.h"
#include "rect.h"
#include "image_subtitle.h"
#include <dcp/subtitle_string.h>
#include <list>

class Image;

class ContentSubtitle
{
public:
	virtual ContentTimePeriod period () const = 0;
};

class ContentImageSubtitle : public ContentSubtitle
{
public:
	ContentImageSubtitle (ContentTimePeriod p, boost::shared_ptr<Image> im, dcpomatic::Rect<double> r)
		: sub (im, r)
		, _period (p)
	{}

	ContentTimePeriod period () const {
		return _period;
	}

	/* Our subtitle, with its rectangle unmodified by any offsets or scales that the content specifies */
	ImageSubtitle sub;

private:
	ContentTimePeriod _period;
};

class ContentTextSubtitle : public ContentSubtitle
{
public:
	ContentTextSubtitle (std::list<dcp::SubtitleString> s)
		: subs (s)
	{}

	ContentTimePeriod period () const;
	
	std::list<dcp::SubtitleString> subs;
};

#endif
