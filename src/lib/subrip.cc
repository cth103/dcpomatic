/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include "subrip.h"
#include "cross.h"
#include "exceptions.h"
#include "subrip_content.h"
#include <sub/subrip_reader.h>
#include <sub/collect.h>

#include "i18n.h"

using std::vector;
using boost::shared_ptr;

SubRip::SubRip (shared_ptr<const SubRipContent> content)
{
	FILE* f = fopen_boost (content->path (0), "r");
	if (!f) {
		throw OpenFileError (content->path (0));
	}

	sub::SubripReader reader (f);
	_subtitles = sub::collect<vector<sub::Subtitle> > (reader.subtitles ());
}

ContentTime
SubRip::length () const
{
	if (_subtitles.empty ()) {
		return ContentTime ();
	}

	return ContentTime::from_seconds (_subtitles.back().to.metric().get().all_as_seconds ());
}
