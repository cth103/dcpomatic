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

#include "sndfile_content.h"
#include "compose.hpp"

#include "i18n.h"

using std::string;
using boost::shared_ptr;

SndfileContent::SndfileContent (boost::filesystem::path f)
	: Content (f)
	, AudioContent (f)
{

}

SndfileContent::SndfileContent (shared_ptr<const cxml::Node> node)
	: Content (node)
	, AudioContent (node)
		   
{

}

string
SndfileContent::summary () const
{
	return String::compose (_("Sound file: %1"), file().filename ());
}

string
SndfileContent::information () const
{
	return "";
}

int
SndfileContent::audio_channels () const
{
	/* XXX */
	return 0;
}

ContentAudioFrame
SndfileContent::audio_length () const
{
	/* XXX */
	return 0;
}

int
SndfileContent::audio_frame_rate () const
{
	/* XXX */
	return 0;
}

int64_t
SndfileContent::audio_channel_layout () const
{
	/* XXX */
	return 0;
}
	

bool
SndfileContent::valid_file (boost::filesystem::path f)
{
	/* XXX: more extensions */
	string ext = f.extension().string();
	transform (ext.begin(), ext.end(), ext.begin(), ::tolower);
	return (ext == ".wav" || ext == ".aif" || ext == ".aiff");
}

shared_ptr<Content>
SndfileContent::clone () const
{
	return shared_ptr<Content> (new SndfileContent (*this));
}
