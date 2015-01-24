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

#ifndef DCPOMATIC_SNDFILE_CONTENT_H
#define DCPOMATIC_SNDFILE_CONTENT_H

#include "single_stream_audio_content.h"
extern "C" {
#include <libavutil/audioconvert.h>
}

namespace cxml {
	class Node;
}

class SndfileContent : public SingleStreamAudioContent
{
public:
	SndfileContent (boost::shared_ptr<const Film>, boost::filesystem::path);
	SndfileContent (boost::shared_ptr<const Film>, cxml::ConstNodePtr, int);

	boost::shared_ptr<SndfileContent> shared_from_this () {
		return boost::dynamic_pointer_cast<SndfileContent> (Content::shared_from_this ());
	}
	
	DCPTime full_length () const;
	
	void examine (boost::shared_ptr<Job>);
	std::string summary () const;
	std::string technical_summary () const;
	std::string information () const;
	void as_xml (xmlpp::Node *) const;
	
	static bool valid_file (boost::filesystem::path);
};

#endif
