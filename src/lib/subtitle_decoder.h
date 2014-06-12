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

#include <dcp/subtitle_string.h>
#include "decoder.h"
#include "rect.h"
#include "types.h"
#include "content_subtitle.h"

class Film;
class DCPTimedSubtitle;
class Image;

class SubtitleDecoder : public virtual Decoder
{
public:
	SubtitleDecoder (boost::shared_ptr<const SubtitleContent>);

	std::list<boost::shared_ptr<ContentImageSubtitle> > get_image_subtitles (ContentTimePeriod period);
	std::list<boost::shared_ptr<ContentTextSubtitle> > get_text_subtitles (ContentTimePeriod period);

protected:
	void seek (ContentTime, bool);
	
	void image_subtitle (ContentTimePeriod period, boost::shared_ptr<Image>, dcpomatic::Rect<double>);
	void text_subtitle (std::list<dcp::SubtitleString>);

	std::list<boost::shared_ptr<ContentImageSubtitle> > _decoded_image_subtitles;
	std::list<boost::shared_ptr<ContentTextSubtitle> > _decoded_text_subtitles;

private:
	template <class T>
	std::list<boost::shared_ptr<T> > get (std::list<boost::shared_ptr<T> > const & subs, ContentTimePeriod period);

	virtual bool has_subtitle_during (ContentTimePeriod) const = 0;
	
	boost::shared_ptr<const SubtitleContent> _subtitle_content;
};

#endif
