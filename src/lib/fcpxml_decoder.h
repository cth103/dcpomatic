/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_FCPXML_DECODER_H
#define DCPOMATIC_FCPXML_DECODER_H


#include "decoder.h"
#include "fcpxml.h"


class FCPXMLContent;


class FCPXMLDecoder : public Decoder
{
public:
	FCPXMLDecoder(std::weak_ptr<const Film> film, std::shared_ptr<const FCPXMLContent> content);

	bool pass() override;
	void seek(dcpomatic::ContentTime time, bool accurate) override;

private:
	void update_position();

	std::shared_ptr<const FCPXMLContent> _fcpxml_content;
	dcpomatic::fcpxml::Sequence _sequence;
	int _next = 0;
};


#endif
