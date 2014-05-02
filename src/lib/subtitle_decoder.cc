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

using std::list;
using std::cout;
using boost::shared_ptr;
using boost::optional;

SubtitleDecoder::SubtitleDecoder ()
{

}

/** Called by subclasses when an image subtitle is ready.
 *  Image may be 0 to say that there is no current subtitle.
 */
void
SubtitleDecoder::image_subtitle (ContentTime from, ContentTime to, shared_ptr<Image> image, dcpomatic::Rect<double> rect)
{
	_decoded_image_subtitles.push_back (shared_ptr<ContentImageSubtitle> (new ContentImageSubtitle (from, to, image, rect)));
}

void
SubtitleDecoder::text_subtitle (list<dcp::SubtitleString> s)
{
	_decoded_text_subtitles.push_back (shared_ptr<ContentTextSubtitle> (new ContentTextSubtitle (s)));
}

template <class T>
list<shared_ptr<T> >
SubtitleDecoder::get (list<shared_ptr<T> > const & subs, ContentTime from, ContentTime to)
{
	if (subs.empty() || from < subs.front()->from() || to > (subs.back()->to() + ContentTime::from_seconds (10))) {
		/* Either we have no decoded data, or what we do have is a long way from what we want: seek */
		seek (from, true);
	}

	/* Now enough pass() calls will either:
	 *  (a) give us what we want, or
	 *  (b) hit the end of the decoder.
	 *
	 *  XXX: with subs being sparse, this may need more care...
	 */
	while (!pass() && (subs.front()->from() > from || to < subs.back()->to())) {}

	/* Now look for what we wanted in the data we have collected */
	/* XXX: inefficient */
	
	list<shared_ptr<T> > out;
	for (typename list<shared_ptr<T> >::const_iterator i = subs.begin(); i != subs.end(); ++i) {
		if ((*i)->from() <= to && (*i)->to() >= from) {
			out.push_back (*i);
		}
	}

	return out;
}

list<shared_ptr<ContentTextSubtitle> >
SubtitleDecoder::get_text_subtitles (ContentTime from, ContentTime to)
{
	return get<ContentTextSubtitle> (_decoded_text_subtitles, from, to);
}

list<shared_ptr<ContentImageSubtitle> >
SubtitleDecoder::get_image_subtitles (ContentTime from, ContentTime to)
{
	return get<ContentImageSubtitle> (_decoded_image_subtitles, from, to);
}

void
SubtitleDecoder::seek (ContentTime, bool)
{
	_decoded_text_subtitles.clear ();
	_decoded_image_subtitles.clear ();
}
