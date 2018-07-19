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

#include "text_caption_file.h"
#include "cross.h"
#include "exceptions.h"
#include "text_caption_file_content.h"
#include <sub/subrip_reader.h>
#include <sub/ssa_reader.h>
#include <sub/collect.h>
#include <unicode/ucsdet.h>
#include <unicode/ucnv.h>
#include <iostream>

#include "i18n.h"

using std::vector;
using std::cout;
using std::string;
using boost::shared_ptr;
using boost::scoped_array;
using boost::optional;
using dcp::Data;

TextCaptionFile::TextCaptionFile (shared_ptr<const TextCaptionFileContent> content)
{
	Data in (content->path (0));

	UErrorCode status = U_ZERO_ERROR;
	UCharsetDetector* detector = ucsdet_open (&status);
	ucsdet_setText (detector, reinterpret_cast<const char *> (in.data().get()), in.size(), &status);

	UCharsetMatch const * match = ucsdet_detect (detector, &status);
	char const * in_charset = ucsdet_getName (match, &status);

	UConverter* to_utf16 = ucnv_open (in_charset, &status);
	/* This is a guess; I think we should be able to encode any input in 4 times its input size */
	scoped_array<uint16_t> utf16 (new uint16_t[in.size() * 2]);
	int const utf16_len = ucnv_toUChars (
		to_utf16, reinterpret_cast<UChar*>(utf16.get()), in.size() * 2,
		reinterpret_cast<const char *> (in.data().get()), in.size(),
		&status
		);

	UConverter* to_utf8 = ucnv_open ("UTF-8", &status);
	/* Another guess */
	scoped_array<char> utf8 (new char[utf16_len * 2]);
	ucnv_fromUChars (to_utf8, utf8.get(), utf16_len * 2, reinterpret_cast<UChar*>(utf16.get()), utf16_len, &status);

	/* Fix OS X line endings */
	size_t utf8_len = strlen (utf8.get ());
	for (size_t i = 0; i < utf8_len; ++i) {
		if (utf8[i] == '\r' && ((i == utf8_len - 1) || utf8[i + 1] != '\n')) {
			utf8[i] = '\n';
		}
	}

	ucsdet_close (detector);
	ucnv_close (to_utf16);
	ucnv_close (to_utf8);

	sub::Reader* reader = 0;

	string ext = content->path(0).extension().string();
	transform (ext.begin(), ext.end(), ext.begin(), ::tolower);

	if (ext == ".srt") {
		reader = new sub::SubripReader (utf8.get());
	} else if (ext == ".ssa" || ext == ".ass") {
		reader = new sub::SSAReader (utf8.get());
	}

	if (reader) {
		_subtitles = sub::collect<vector<sub::Subtitle> > (reader->subtitles ());
	}

	delete reader;
}

/** @return time of first subtitle, if there is one */
optional<ContentTime>
TextCaptionFile::first () const
{
	if (_subtitles.empty()) {
		return optional<ContentTime>();
	}

	return ContentTime::from_seconds(_subtitles[0].from.all_as_seconds());
}

ContentTime
TextCaptionFile::length () const
{
	if (_subtitles.empty ()) {
		return ContentTime ();
	}

	return ContentTime::from_seconds (_subtitles.back().to.all_as_seconds ());
}
