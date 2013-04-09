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
#include "imagemagick_content.h"
#include "imagemagick_decoder.h"
#include "compose.hpp"

#include "i18n.h"

using std::string;
using std::stringstream;
using boost::shared_ptr;

ImageMagickContent::ImageMagickContent (boost::filesystem::path f)
	: Content (f)
	, VideoContent (f)
{

}

ImageMagickContent::ImageMagickContent (shared_ptr<const cxml::Node> node)
	: Content (node)
	, VideoContent (node)
{
	
}

string
ImageMagickContent::summary () const
{
	return String::compose (_("Image: %1"), file().filename().string());
}

bool
ImageMagickContent::valid_file (boost::filesystem::path f)
{
	string ext = f.extension().string();
	transform (ext.begin(), ext.end(), ext.begin(), ::tolower);
	return (ext == ".tif" || ext == ".tiff" || ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp");
}

void
ImageMagickContent::as_xml (xmlpp::Node* node) const
{
	node->add_child("Type")->add_child_text ("ImageMagick");
	Content::as_xml (node);
	VideoContent::as_xml (node);
}

void
ImageMagickContent::examine (shared_ptr<Film> film, shared_ptr<Job> job, bool quick)
{
	Content::examine (film, job, quick);
	shared_ptr<ImageMagickDecoder> decoder (new ImageMagickDecoder (film, shared_from_this()));

	{
		boost::mutex::scoped_lock lm (_mutex);
		/* Initial length */
		_video_length = 10 * 24;
	}
	
	take_from_video_decoder (decoder);
	
        signal_changed (VideoContentProperty::VIDEO_LENGTH);
}

shared_ptr<Content>
ImageMagickContent::clone () const
{
	return shared_ptr<Content> (new ImageMagickContent (*this));
}

void
ImageMagickContent::set_video_length (ContentVideoFrame len)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_video_length = len;
	}

	signal_changed (VideoContentProperty::VIDEO_LENGTH);
}
