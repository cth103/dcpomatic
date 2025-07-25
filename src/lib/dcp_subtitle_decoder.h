/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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


#include "dcp_subtitle.h"
#include "font_id_allocator.h"
#include "text_decoder.h"


class DCPSubtitleContent;


class DCPSubtitleDecoder : public DCPSubtitle, public Decoder
{
public:
	DCPSubtitleDecoder(std::shared_ptr<const Film> film, std::shared_ptr<const DCPSubtitleContent>);

	bool pass() override;
	void seek(dcpomatic::ContentTime time, bool accurate) override;

	boost::optional<dcpomatic::ContentTime> first() const;

private:
	dcpomatic::ContentTimePeriod content_time_period(std::shared_ptr<const dcp::Text> s) const;
	void update_position();

	std::vector<std::shared_ptr<const dcp::Text>> _subtitles;
	std::vector<std::shared_ptr<const dcp::Text>>::const_iterator _next;

	dcp::SubtitleStandard _subtitle_standard;

	std::shared_ptr<dcp::TextAsset> _asset;
	FontIDAllocator _font_id_allocator;
};
