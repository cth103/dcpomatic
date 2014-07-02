/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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
#include "subtitle_content.h"

using std::list;
using std::cout;
using boost::shared_ptr;
using boost::optional;

SubtitleDecoder::SubtitleDecoder (shared_ptr<const SubtitleContent> c)
	: _subtitle_content (c)
{

}

/** Called by subclasses when an image subtitle is ready.
 *  Image may be 0 to say that there is no current subtitle.
 */
void
SubtitleDecoder::image_subtitle (ContentTimePeriod period, shared_ptr<Image> image, dcpomatic::Rect<double> rect)
{
	_decoded_image_subtitles.push_back (ContentImageSubtitle (period, image, rect));
}

void
SubtitleDecoder::text_subtitle (list<dcp::SubtitleString> s)
{
	_decoded_text_subtitles.push_back (ContentTextSubtitle (s));
}

template <class T>
list<T>
SubtitleDecoder::get (list<T> const & subs, ContentTimePeriod period)
{
	if (!has_subtitle_during (period)) {
		return list<T> ();
	}

	if (subs.empty() || period.from < subs.front().period().from || period.to > (subs.back().period().to + ContentTime::from_seconds (10))) {
		/* Either we have no decoded data, or what we do have is a long way from what we want: seek */
		seek (period.from, true);
	}

	/* Now enough pass() calls will either:
	 *  (a) give us what we want, or
	 *  (b) hit the end of the decoder.
	 */
	while (!pass() && (subs.empty() || (subs.front().period().from > period.from || period.to < subs.back().period().to))) {}

	/* Now look for what we wanted in the data we have collected */
	/* XXX: inefficient */
	
	list<T> out;
	for (typename list<T>::const_iterator i = subs.begin(); i != subs.end(); ++i) {
		if (i->period().overlaps (period)) {
			out.push_back (*i);
		}
	}

	return out;
}

list<ContentTextSubtitle>
SubtitleDecoder::get_text_subtitles (ContentTimePeriod period)
{
	return get<ContentTextSubtitle> (_decoded_text_subtitles, period);
}

list<ContentImageSubtitle>
SubtitleDecoder::get_image_subtitles (ContentTimePeriod period)
{
	return get<ContentImageSubtitle> (_decoded_image_subtitles, period);
}

void
SubtitleDecoder::seek (ContentTime, bool)
{
	_decoded_text_subtitles.clear ();
	_decoded_image_subtitles.clear ();
}
