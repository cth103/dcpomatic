/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

class DCPTextContent;

class DCPTextDecoder : public DCPSubtitle, public Decoder
{
public:
	DCPTextDecoder (boost::shared_ptr<const DCPTextContent>, boost::shared_ptr<Log> log);

	bool pass ();
	void seek (ContentTime time, bool accurate);

private:
	ContentTimePeriod content_time_period (boost::shared_ptr<dcp::Subtitle> s) const;

	std::list<boost::shared_ptr<dcp::Subtitle> > _subtitles;
	std::list<boost::shared_ptr<dcp::Subtitle> >::const_iterator _next;
};
