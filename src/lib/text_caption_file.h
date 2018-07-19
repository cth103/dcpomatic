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

#ifndef DCPOMATIC_TEXT_CAPTION_FILE_H
#define DCPOMATIC_TEXT_CAPTION_FILE_H

#include "dcpomatic_time.h"
#include <sub/subtitle.h>
#include <boost/shared_ptr.hpp>
#include <vector>

class TextCaptionFileContent;
class plain_text_time_test;
class plain_text_coordinate_test;
class plain_text_content_test;
class plain_text_parse_test;

/** @class TextCaptionFile
 *  @brief Base for TextCaptionFile decoder and examiner.
 *
 *  In fact this is sufficient for the examiner, so it's used as-is rather than deriving
 *  a pointless TextCaptionFileExaminer.
 */
class TextCaptionFile
{
public:
	explicit TextCaptionFile (boost::shared_ptr<const TextCaptionFileContent>);

	boost::optional<ContentTime> first () const;
	ContentTime length () const;

protected:
	std::vector<sub::Subtitle> _subtitles;
};

#endif
