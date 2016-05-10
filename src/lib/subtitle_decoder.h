/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_SUBTITLE_DECODER_H
#define DCPOMATIC_SUBTITLE_DECODER_H

#include "decoder.h"
#include "rect.h"
#include "types.h"
#include "content_subtitle.h"
#include <dcp/subtitle_string.h>

class Image;

class SubtitleDecoder
{
public:
	/** Second parameter to the _during functions is true if we
	 *  want only subtitles that start during the period,
	 *  otherwise we want subtitles that overlap the period.
	 */
	SubtitleDecoder (
		Decoder* parent,
		boost::shared_ptr<const SubtitleContent>,
		boost::function<std::list<ContentTimePeriod> (ContentTimePeriod, bool)> image_during,
		boost::function<std::list<ContentTimePeriod> (ContentTimePeriod, bool)> text_during
		);

	std::list<ContentImageSubtitle> get_image (ContentTimePeriod period, bool starting, bool accurate);
	std::list<ContentTextSubtitle> get_text (ContentTimePeriod period, bool starting, bool accurate);

	void seek (ContentTime, bool);

	void give_image (ContentTimePeriod period, boost::shared_ptr<Image>, dcpomatic::Rect<double>);
	void give_text (ContentTimePeriod period, std::list<dcp::SubtitleString>);

	boost::shared_ptr<const SubtitleContent> content () const {
		return _content;
	}

private:
	Decoder* _parent;
	std::list<ContentImageSubtitle> _decoded_image;
	std::list<ContentTextSubtitle> _decoded_text;
	boost::shared_ptr<const SubtitleContent> _content;

	template <class T>
	std::list<T> get (std::list<T> const & subs, std::list<ContentTimePeriod> const & sp, ContentTimePeriod period, bool starting, bool accurate);

	boost::function<std::list<ContentTimePeriod> (ContentTimePeriod, bool)> _image_during;
	boost::function<std::list<ContentTimePeriod> (ContentTimePeriod, bool)> _text_during;
};

#endif
