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
#include "ffmpeg_content.h"
#include "image_content.h"
#include "sndfile_content.h"
#include "util.h"

using std::string;
using boost::shared_ptr;

shared_ptr<Content>
content_factory (shared_ptr<const Film> film, cxml::NodePtr node, int version)
{
	string const type = node->string_child ("Type");

	boost::shared_ptr<Content> content;
	
	if (type == "FFmpeg") {
		content.reset (new FFmpegContent (film, node, version));
	} else if (type == "Image") {
		content.reset (new ImageContent (film, node, version));
	} else if (type == "Sndfile") {
		content.reset (new SndfileContent (film, node, version));
	}

	return content;
}

shared_ptr<Content>
content_factory (shared_ptr<const Film> film, boost::filesystem::path path)
{
	shared_ptr<Content> content;
		
	if (valid_image_file (path)) {
		content.reset (new ImageContent (film, path));
	} else if (SndfileContent::valid_file (path)) {
		content.reset (new SndfileContent (film, path));
	} else {
		content.reset (new FFmpegContent (film, path));
	}

	return content;
}
