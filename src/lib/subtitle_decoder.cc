/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <boost/shared_ptr.hpp>
#include "subtitle_decoder.h"
#include "subtitle.h"

using boost::shared_ptr;

SubtitleDecoder::SubtitleDecoder (shared_ptr<const Film> f)
	: Decoder (f)
{

}


/** Called by subclasses when a subtitle is ready.
 *  s may be 0 to say that there is no current subtitle.
 *  @param s New current subtitle, or 0.
 */
void
SubtitleDecoder::subtitle (shared_ptr<TimedSubtitle> s)
{
	Subtitle (s);
}
