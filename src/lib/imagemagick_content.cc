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
#include "imagemagick_examiner.h"
#include "config.h"
#include "compose.hpp"
#include "film.h"

#include "i18n.h"

using std::string;
using std::stringstream;
using boost::shared_ptr;

ImageMagickContent::ImageMagickContent (shared_ptr<const Film> f, boost::filesystem::path p)
	: Content (f, p)
	, VideoContent (f, p)
{

}

ImageMagickContent::ImageMagickContent (shared_ptr<const Film> f, shared_ptr<const cxml::Node> node)
	: Content (f, node)
	, VideoContent (f, node)
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
ImageMagickContent::examine (shared_ptr<Job> job)
{
	Content::examine (job);

	shared_ptr<const Film> film = _film.lock ();
	assert (film);
	
	shared_ptr<ImageMagickExaminer> examiner (new ImageMagickExaminer (film, shared_from_this()));

	set_video_length (Config::instance()->default_still_length() * video_frame_rate());
	take_from_video_examiner (examiner);
}

void
ImageMagickContent::set_video_length (VideoContent::Frame len)
{
	{
		boost::mutex::scoped_lock lm (_mutex);
		_video_length = len;
	}

	signal_changed (ContentProperty::LENGTH);
}

Time
ImageMagickContent::length () const
{
	shared_ptr<const Film> film = _film.lock ();
	assert (film);
	
	FrameRateConversion frc (video_frame_rate(), film->dcp_video_frame_rate ());
	return video_length() * frc.factor() * TIME_HZ / video_frame_rate();
}

string
ImageMagickContent::identifier () const
{
	stringstream s;
	s << VideoContent::identifier ();
	s << "_" << video_length();
	return s.str ();
}
