/*
    Copyright (C) 2016 Carl Hetherington <cth@carlh.net>

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

#include "timeline_atmos_content_view.h"

using boost::shared_ptr;

/** @class TimelineAtmosContentView
 *  @brief Timeline view for AtmosContent.
 */

TimelineAtmosContentView::TimelineAtmosContentView (Timeline& tl, shared_ptr<Content> c)
	: TimelineContentView (tl, c)
{

}

wxColour
TimelineAtmosContentView::background_colour () const
{
	return wxColour (149, 121, 232, 255);
}

wxColour
TimelineAtmosContentView::foreground_colour () const
{
	return wxColour (0, 0, 0, 255);
}
