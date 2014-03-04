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

using std::list;
using boost::shared_ptr;
using boost::optional;

SubtitleDecoder::SubtitleDecoder (shared_ptr<const Film> f)
	: Decoder (f)
{

}


/** Called by subclasses when an image subtitle is ready.
 *  Image may be 0 to say that there is no current subtitle.
 */
void
SubtitleDecoder::image_subtitle (shared_ptr<Image> image, dcpomatic::Rect<double> rect, ContentTime from, ContentTime to)
{
	_pending.push_back (shared_ptr<DecodedImageSubtitle> (new DecodedImageSubtitle (from, to, image, rect)));
}

void
SubtitleDecoder::text_subtitle (list<dcp::SubtitleString> s)
{
	_pending.push_back (shared_ptr<DecodedTextSubtitle> (new DecodedTextSubtitle (s)));
}
