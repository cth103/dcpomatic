/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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
 *  @param period Period of the subtitle.
 *  @param image Subtitle image.
 *  @param rect Area expressed as a fraction of the video frame that this subtitle
 *  is for (e.g. a width of 0.5 means the width of the subtitle is half the width
 *  of the video frame)
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

/** @param sp Full periods of subtitles that are showing or starting during the specified period */
template <class T>
list<T>
SubtitleDecoder::get (list<T> const & subs, list<ContentTimePeriod> const & sp, ContentTimePeriod period, bool starting)
{
	if (sp.empty ()) {
		/* Nothing in this period */
		return list<T> ();
	}

	/* Seek if what we want is before what we have, or more than a reasonable amount after */
	if (subs.empty() || sp.back().to < subs.front().period().from || sp.front().from > (subs.back().period().to + ContentTime::from_seconds (5))) {
		seek (sp.front().from, true);
	}

	/* Now enough pass() calls will either:
	 *  (a) give us what we want, or
	 *  (b) hit the end of the decoder.
	 */
	while (!pass(PASS_REASON_SUBTITLE) && (subs.empty() || (subs.back().period().to < sp.back().to))) {}

	/* Now look for what we wanted in the data we have collected */
	/* XXX: inefficient */
	
	list<T> out;
	for (typename list<T>::const_iterator i = subs.begin(); i != subs.end(); ++i) {
		if ((starting && period.contains (i->period().from)) || (!starting && period.overlaps (i->period ()))) {
			out.push_back (*i);
		}
	}

	/* XXX: should clear out _decoded_* at some point */

	return out;
}

list<ContentTextSubtitle>
SubtitleDecoder::get_text_subtitles (ContentTimePeriod period, bool starting)
{
	return get<ContentTextSubtitle> (_decoded_text_subtitles, text_subtitles_during (period, starting), period, starting);
}

list<ContentImageSubtitle>
SubtitleDecoder::get_image_subtitles (ContentTimePeriod period, bool starting)
{
	return get<ContentImageSubtitle> (_decoded_image_subtitles, image_subtitles_during (period, starting), period, starting);
}

void
SubtitleDecoder::seek (ContentTime, bool)
{
	_decoded_text_subtitles.clear ();
	_decoded_image_subtitles.clear ();
}
