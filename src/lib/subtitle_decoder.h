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

#ifndef DCPOMATIC_SUBTITLE_DECODER_H
#define DCPOMATIC_SUBTITLE_DECODER_H

#include "decoder.h"
#include "rect.h"
#include "types.h"
#include "content_subtitle.h"
#include <dcp/subtitle_string.h>

class Film;
class DCPTimedSubtitle;
class Image;

class SubtitleDecoder : public virtual Decoder
{
public:
	SubtitleDecoder (boost::shared_ptr<const SubtitleContent>);

	std::list<ContentImageSubtitle> get_image_subtitles (ContentTimePeriod period, bool starting);
	std::list<ContentTextSubtitle> get_text_subtitles (ContentTimePeriod period, bool starting);

protected:
	void seek (ContentTime, bool);
	
	void image_subtitle (ContentTimePeriod period, boost::shared_ptr<Image>, dcpomatic::Rect<double>);
	void text_subtitle (std::list<dcp::SubtitleString>);

	std::list<ContentImageSubtitle> _decoded_image_subtitles;
	std::list<ContentTextSubtitle> _decoded_text_subtitles;

private:
	template <class T>
	std::list<T> get (std::list<T> const & subs, std::list<ContentTimePeriod> const & sp, ContentTimePeriod period, bool starting);

	/** @param starting true if we want only subtitles that start during the period, otherwise
	 *  we want subtitles that overlap the period.
	 */
	virtual std::list<ContentTimePeriod> image_subtitles_during (ContentTimePeriod period, bool starting) const = 0;
	virtual std::list<ContentTimePeriod> text_subtitles_during (ContentTimePeriod period, bool starting) const = 0;
	
	boost::shared_ptr<const SubtitleContent> _subtitle_content;
};

#endif
