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

#include "audio_content.h"

namespace cxml {
	class Node;
}

class SndfileContent : public AudioContent
{
public:
	SndfileContent (boost::filesystem::path);
	SndfileContent (boost::shared_ptr<const cxml::Node>);
	
	std::string summary () const;
	std::string information () const;
	boost::shared_ptr<Content> clone () const;

        /* AudioContent */
        int audio_channels () const;
        ContentAudioFrame audio_length () const;
        int audio_frame_rate () const;
        int64_t audio_channel_layout () const;

	static bool valid_file (boost::filesystem::path);
};
