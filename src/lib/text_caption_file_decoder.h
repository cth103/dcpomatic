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

#ifndef DCPOMATIC_PLAIN_TEXT_FILE_DECODER_H
#define DCPOMATIC_PLAIN_TEXT_FILE_DECODER_H

#include "plain_text_file.h"
#include "decoder.h"

class TextCaptionFileContent;
class Log;

class TextCaptionFileDecoder : public Decoder, public TextCaptionFile
{
public:
	TextCaptionFileDecoder (boost::shared_ptr<const TextCaptionFileContent>, boost::shared_ptr<Log> log);

	void seek (ContentTime time, bool accurate);
	bool pass ();

private:
	ContentTimePeriod content_time_period (sub::Subtitle s) const;

	size_t _next;
};

#endif
