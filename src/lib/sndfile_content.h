/* -*- c-basic-offset: 8; default-tab-width: 8; -*- */

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

extern "C" {
#include <libavutil/audioconvert.h>
}
#include "audio_content.h"

namespace cxml {
	class Node;
}

class SndfileContent : public AudioContent
{
public:
	SndfileContent (boost::filesystem::path);
	SndfileContent (boost::shared_ptr<const cxml::Node>);

	boost::shared_ptr<SndfileContent> shared_from_this () {
		return boost::dynamic_pointer_cast<SndfileContent> (Content::shared_from_this ());
	}
	
	void examine (boost::shared_ptr<Film>, boost::shared_ptr<Job>, bool);
	std::string summary () const;
	std::string information () const;
	void as_xml (xmlpp::Node *) const;
	boost::shared_ptr<Content> clone () const;
	Time length (boost::shared_ptr<const Film>) const;

        /* AudioContent */
        int audio_channels () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _audio_channels;
	}
	
        ContentAudioFrame audio_length () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _audio_length;
	}
	
        int content_audio_frame_rate () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _audio_frame_rate;
	}

        int output_audio_frame_rate (boost::shared_ptr<const Film>) const;
	
	static bool valid_file (boost::filesystem::path);

private:
	int _audio_channels;
	ContentAudioFrame _audio_length;
	int _audio_frame_rate;
};
