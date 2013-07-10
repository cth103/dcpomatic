/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_SUBTITLE_CONTENT_H
#define DCPOMATIC_SUBTITLE_CONTENT_H

#include "content.h"

class SubtitleContentProperty
{
public:
	static int const SUBTITLE_OFFSET;
	static int const SUBTITLE_SCALE;
};

class SubtitleContent : public virtual Content
{
public:
	SubtitleContent (boost::shared_ptr<const Film>, boost::filesystem::path);
	SubtitleContent (boost::shared_ptr<const Film>, boost::shared_ptr<const cxml::Node>);
	
	void as_xml (xmlpp::Node *) const;

	void set_subtitle_offset (int);
	void set_subtitle_scale (float);

	int subtitle_offset () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _subtitle_offset;
	}

	float subtitle_scale () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _subtitle_scale;
	}
	
private:	
	/** y offset for placing subtitles, in source pixels; +ve is further down
	    the frame, -ve is further up.
	*/
	int _subtitle_offset;
	/** scale factor to apply to subtitles */
	float _subtitle_scale;
};

#endif
