/*
    Copyright (C) 2014-2016 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_TEXT_SUBTITLE_DECODER_H
#define DCPOMATIC_TEXT_SUBTITLE_DECODER_H

#include "text_decoder.h"
#include "plain_text.h"

class PlainText;

class PlainTextDecoder : public Decoder, public PlainText
{
public:
	PlainTextDecoder (boost::shared_ptr<const PlainTextContent>, boost::shared_ptr<Log> log);

	void seek (ContentTime time, bool accurate);
	bool pass ();

private:
	ContentTimePeriod content_time_period (sub::Subtitle s) const;

	size_t _next;
};

#endif
