/*
    Copyright (C) 2014-2018 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_TEXT_CAPTION_FILE_DECODER_H
#define DCPOMATIC_TEXT_CAPTION_FILE_DECODER_H

#include "string_text_file.h"
#include "decoder.h"

class StringTextFileContent;

class StringTextFileDecoder : public Decoder, public StringTextFile
{
public:
	StringTextFileDecoder(std::shared_ptr<const Film> film, std::shared_ptr<const StringTextFileContent>);

	void seek(dcpomatic::ContentTime time, bool accurate) override;
	bool pass() override;

private:
	dcpomatic::ContentTimePeriod content_time_period(sub::Subtitle s) const;
	void update_position();

	size_t _next;
};

#endif
