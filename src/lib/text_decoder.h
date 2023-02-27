/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_CAPTION_DECODER_H
#define DCPOMATIC_CAPTION_DECODER_H


#include "content_text.h"
#include "decoder.h"
#include "decoder_part.h"
#include "rect.h"
#include "content_text.h"
#include "types.h"
#include <dcp/subtitle_standard.h>
#include <dcp/subtitle_string.h>
#include <boost/signals2.hpp>


namespace sub {
	class Subtitle;
}

class Image;


class TextDecoder : public DecoderPart
{
public:
	TextDecoder (Decoder* parent, std::shared_ptr<const TextContent>);

	boost::optional<dcpomatic::ContentTime> position (std::shared_ptr<const Film>) const override {
		return _position;
	}

	void emit_bitmap_start (ContentBitmapText const& bitmap);
	void emit_bitmap (dcpomatic::ContentTimePeriod period, std::shared_ptr<const Image> image, dcpomatic::Rect<double> rect);
	void emit_plain_start(dcpomatic::ContentTime from, std::vector<dcp::SubtitleString> s, dcp::SubtitleStandard valign_standard);
	void emit_plain_start (dcpomatic::ContentTime from, sub::Subtitle const & subtitle);
	void emit_plain(dcpomatic::ContentTimePeriod period, std::vector<dcp::SubtitleString> s, dcp::SubtitleStandard valign_standard);
	void emit_plain (dcpomatic::ContentTimePeriod period, sub::Subtitle const & subtitle);
	void emit_stop (dcpomatic::ContentTime to);

	void maybe_set_position (dcpomatic::ContentTime position);

	void seek () override;

	std::shared_ptr<const TextContent> content () const {
		return _content;
	}

	static std::string remove_invalid_characters_for_xml(std::string text);

	boost::signals2::signal<void (ContentBitmapText)> BitmapStart;
	boost::signals2::signal<void (ContentStringText)> PlainStart;
	boost::signals2::signal<void (dcpomatic::ContentTime)> Stop;

private:
	std::shared_ptr<const TextContent> _content;
	boost::optional<dcpomatic::ContentTime> _position;
};


#endif
