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

#include "position_image.h"
#include <dcp/subtitle_string.h>
#include <dcp/util.h>

class Font;

std::string marked_up (std::list<dcp::SubtitleString> subtitles);
std::list<PositionImage> render_subtitles (std::list<dcp::SubtitleString>, std::list<boost::shared_ptr<Font> > fonts, dcp::Size);
