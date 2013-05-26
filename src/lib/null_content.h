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

#include <string>
#include <boost/shared_ptr.hpp>
#include "video_content.h"
#include "audio_content.h"

class NullContent : public VideoContent, public AudioContent
{
public:
	NullContent (boost::shared_ptr<const Film>, Time, Time);

	std::string summary () const {
		return "";
	}
	
	std::string information () const {
		return "";
	}

	void as_xml (xmlpp::Node *) const {}
	
	boost::shared_ptr<Content> clone () const {
		return boost::shared_ptr<Content> ();
	}

        int audio_channels () const;
	
        ContentAudioFrame audio_length () const {
		return _audio_length;
	}
	
        int content_audio_frame_rate () const;
	
	int output_audio_frame_rate () const;
	
	AudioMapping audio_mapping () const {
		return AudioMapping ();
	}

	void set_audio_mapping (AudioMapping) {}
	
	Time length () const {
		return _length;
	}

private:
	ContentAudioFrame _audio_length;
	Time _length;
};
