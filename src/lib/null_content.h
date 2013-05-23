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
#include "content.h"

class NullContent : public VideoContent, public AudioContent
{
public:
	NullContent (Time, Time, boost::shared_ptr<const Film>);

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

        int audio_channels () const {
		return MAX_AUDIO_CHANNELS;
	}
	
        ContentAudioFrame audio_length () const {
		return _audio_length;
	}
	
        int content_audio_frame_rate () const {
		return _content_audio_frame_rate;
	}
	
	int output_audio_frame_rate (boost::shared_ptr<const Film> f) const {
		return f->dcp_audio_frame_rate ();
	}
	
	AudioMapping audio_mapping () const {
		return AudioMapping ();
	}
	
	Time length (boost::shared_ptr<const Film>) const {
		return _length;
	}

private:
	ContentAudioFrame _audio_length;
	ContentAudioFrame _content_audio_frame_rate;
};
