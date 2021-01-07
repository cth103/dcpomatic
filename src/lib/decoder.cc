/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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

#include "decoder.h"
#include "video_decoder.h"
#include "audio_decoder.h"
#include "text_decoder.h"
#include <boost/optional.hpp>
#include <iostream>

using std::cout;
using boost::optional;
using std::shared_ptr;
using std::weak_ptr;
using namespace dcpomatic;

Decoder::Decoder (weak_ptr<const Film> film)
	: WeakConstFilm (film)
{

}

/** @return Earliest time of content that the next pass() will emit */
ContentTime
Decoder::position () const
{
	optional<ContentTime> pos;
	shared_ptr<const Film> f = film();

	if (video && !video->ignore() && (!pos || video->position(f).get_value_or(ContentTime()) < *pos)) {
		pos = video->position(f);
	}

	if (audio && !audio->ignore() && (!pos || audio->position(f).get_value_or(ContentTime()) < *pos)) {
		pos = audio->position(f);
	}

	/* Only decide position based on subtitle sources if there is nothing else
	   to go on.  Otherwise we can have problems with muxed sources which have
	   (for example) video, audio and a subtitle.  If the subtitle data runs out
	   before the video/audio the position() call will return the position of the
	   end of the subs.  This causes this file to be pass()ed in favour of others,
	   which can cause bugs like #1581.
	*/
	if (!pos) {
		BOOST_FOREACH (shared_ptr<TextDecoder> i, text) {
			if (!i->ignore() && (!pos || i->position(f) < *pos)) {
				pos = i->position(f);
			}
		}
	}

	return pos.get_value_or(ContentTime());
}

void
Decoder::seek (ContentTime, bool)
{
	if (video) {
		video->seek ();
	}
	if (audio) {
		audio->seek ();
	}
	BOOST_FOREACH (shared_ptr<TextDecoder> i, text) {
		i->seek ();
	}
}

shared_ptr<TextDecoder>
Decoder::only_text () const
{
	DCPOMATIC_ASSERT (text.size() < 2);
	if (text.empty ()) {
		return shared_ptr<TextDecoder> ();
	}
	return text.front ();
}
