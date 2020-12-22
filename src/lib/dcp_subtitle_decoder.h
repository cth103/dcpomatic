/*
    Copyright (C) 2014-2020 Carl Hetherington <cth@carlh.net>

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

#include "text_decoder.h"
#include "dcp_subtitle.h"
#include "font_data.h"

class DCPSubtitleContent;

class DCPSubtitleDecoder : public DCPSubtitle, public Decoder
{
public:
	DCPSubtitleDecoder (boost::shared_ptr<const Film> film, boost::shared_ptr<const DCPSubtitleContent>);

	bool pass ();
	void seek (dcpomatic::ContentTime time, bool accurate);

	std::vector<dcpomatic::FontData> fonts () const;

private:
	dcpomatic::ContentTimePeriod content_time_period (boost::shared_ptr<dcp::Subtitle> s) const;

	std::list<boost::shared_ptr<dcp::Subtitle> > _subtitles;
	std::list<boost::shared_ptr<dcp::Subtitle> >::const_iterator _next;

	std::vector<dcpomatic::FontData> _fonts;
};
