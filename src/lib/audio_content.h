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

#ifndef DCPOMATIC_AUDIO_CONTENT_H
#define DCPOMATIC_AUDIO_CONTENT_H

#include "content.h"
#include "util.h"

namespace cxml {
	class Node;
}

class AudioContentProperty
{
public:
	static int const AUDIO_CHANNELS;
	static int const AUDIO_LENGTH;
	static int const AUDIO_FRAME_RATE;
};

class AudioContent : public virtual Content
{
public:
	AudioContent (boost::filesystem::path);
	AudioContent (boost::shared_ptr<const cxml::Node>);
	AudioContent (AudioContent const &);

        virtual int audio_channels () const = 0;
        virtual ContentAudioFrame audio_length () const = 0;
        virtual int audio_frame_rate () const = 0;
};

#endif
