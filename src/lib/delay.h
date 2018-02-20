/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_DELAY_H
#define DCPOMATIC_DELAY_H

#include "types.h"
#include "video_adjuster.h"
#include "content_video.h"
#include <boost/signals2.hpp>

class Piece;

/** A class which `delays' received video by 2 frames; i.e. when it receives
 *  a video frame it emits the last-but-one it received.
 */
class Delay : public VideoAdjuster
{
public:
	void video (boost::weak_ptr<Piece>, ContentVideo video);

private:
	boost::optional<ContentVideo> _last;
};

#endif
