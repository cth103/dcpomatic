/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_SUBTITLE_DECODER_H
#define DCPOMATIC_SUBTITLE_DECODER_H

#include "decoder.h"
#include "rect.h"
#include "types.h"
#include "content_subtitle.h"
#include "decoder_part.h"
#include <dcp/subtitle_string.h>
#include <boost/signals2.hpp>

namespace sub {
	class Subtitle;
}

class Image;

class SubtitleDecoder : public DecoderPart
{
public:
	/** Second parameter to the _during functions is true if we
	 *  want only subtitles that start during the period,
	 *  otherwise we want subtitles that overlap the period.
	 */
	SubtitleDecoder (
		Decoder* parent,
		boost::shared_ptr<const SubtitleContent>,
		boost::shared_ptr<Log> log
		);

	void emit_image (ContentTimePeriod period, boost::shared_ptr<Image>, dcpomatic::Rect<double>);
	void emit_text (ContentTimePeriod period, std::list<dcp::SubtitleString>);
	void emit_text (ContentTimePeriod period, sub::Subtitle const & subtitle);

	boost::shared_ptr<const SubtitleContent> content () const {
		return _content;
	}

	boost::signals2::signal<void (ContentImageSubtitle)> ImageData;
	boost::signals2::signal<void (ContentTextSubtitle)> TextData;

private:
	boost::shared_ptr<const SubtitleContent> _content;
};

#endif
