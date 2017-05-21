/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#include "types.h"
#include "dcpomatic_time.h"

class ContentPart;

/** @return Pieces of content with a given part (video, audio,
 *  subtitle) that overlap a specified time range in the given
 *  ContentList
 */
ContentList overlaps (
	ContentList cl, boost::function<boost::shared_ptr<ContentPart> (boost::shared_ptr<const Content>)> part, DCPTime from, DCPTime to
	);
