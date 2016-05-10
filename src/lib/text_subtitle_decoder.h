/*
    Copyright (C) 2014-2016 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_TEXT_SUBTITLE_DECODER_H
#define DCPOMATIC_TEXT_SUBTITLE_DECODER_H

#include "subtitle_decoder.h"
#include "text_subtitle.h"

class TextSubtitleContent;

class TextSubtitleDecoder : public Decoder, public TextSubtitle
{
public:
	TextSubtitleDecoder (boost::shared_ptr<const TextSubtitleContent>);

protected:
	void seek (ContentTime time, bool accurate);
	bool pass (PassReason, bool accurate);

private:
	std::list<ContentTimePeriod> image_subtitles_during (ContentTimePeriod, bool starting) const;
	std::list<ContentTimePeriod> text_subtitles_during (ContentTimePeriod, bool starting) const;
	ContentTimePeriod content_time_period (sub::Subtitle s) const;

	size_t _next;
};

#endif
