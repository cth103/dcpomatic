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

#include <libcxml/cxml.h>
#include "audio_content.h"

using boost::shared_ptr;

int const AudioContentProperty::AUDIO_CHANNELS = 200;
int const AudioContentProperty::AUDIO_LENGTH = 201;
int const AudioContentProperty::AUDIO_FRAME_RATE = 202;

AudioContent::AudioContent (boost::filesystem::path f)
	: Content (f)
{

}

AudioContent::AudioContent (shared_ptr<const cxml::Node> node)
	: Content (node)
{

}

AudioContent::AudioContent (AudioContent const & o)
	: Content (o)
{

}
