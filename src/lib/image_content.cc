/*
    Copyright (C) 2013-2015 Carl Hetherington <cth@carlh.net>

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

#include "image_content.h"
#include "video_content.h"
#include "image_examiner.h"
#include "compose.hpp"
#include "film.h"
#include "job.h"
#include "frame_rate_change.h"
#include "exceptions.h"
#include "safe_stringstream.h"
#include "image_filename_sorter.h"
#include <libcxml/cxml.h>
#include <libxml++/libxml++.h>
#include <boost/foreach.hpp>
#include <iostream>

#include "i18n.h"

using std::string;
using std::cout;
using boost::shared_ptr;

ImageContent::ImageContent (shared_ptr<const Film> film, boost::filesystem::path p)
	: Content (film)
{
	video.reset (new VideoContent (this, film));

	if (boost::filesystem::is_regular_file (p) && valid_image_file (p)) {
		_paths.push_back (p);
	} else {
		for (boost::filesystem::directory_iterator i(p); i != boost::filesystem::directory_iterator(); ++i) {
			if (boost::filesystem::is_regular_file (i->path()) && valid_image_file (i->path())) {
				_paths.push_back (i->path ());
			}
		}

		if (_paths.empty()) {
			throw FileError (_("No valid image files were found in the folder."), p);
		}

		sort (_paths.begin(), _paths.end(), ImageFilenameSorter ());
	}

	set_default_colour_conversion ();
}


ImageContent::ImageContent (shared_ptr<const Film> film, cxml::ConstNodePtr node, int version)
	: Content (film, node)
{
	video = VideoContent::from_xml (this, film, node, version);
}

string
ImageContent::summary () const
{
	string s = path_summary () + " ";
	/* Get the string() here so that the name does not have quotes around it */
	if (still ()) {
		s += _("[still]");
	} else {
		s += _("[moving images]");
	}

	return s;
}

string
ImageContent::technical_summary () const
{
	string s = Content::technical_summary() + " - "
		+ video->technical_summary() + " - ";

	if (still ()) {
		s += _("still");
	} else {
		s += _("moving");
	}

	return s;
}

void
ImageContent::as_xml (xmlpp::Node* node) const
{
	node->add_child("Type")->add_child_text ("Image");
	Content::as_xml (node);

	if (video) {
		video->as_xml (node);
	}
}

void
ImageContent::examine (shared_ptr<Job> job)
{
	Content::examine (job);

	shared_ptr<const Film> film = _film.lock ();
	DCPOMATIC_ASSERT (film);

	shared_ptr<ImageExaminer> examiner (new ImageExaminer (film, shared_from_this(), job));
	video->take_from_examiner (examiner);
	set_default_colour_conversion ();
}

DCPTime
ImageContent::full_length () const
{
	shared_ptr<const Film> film = _film.lock ();
	DCPOMATIC_ASSERT (film);
	FrameRateChange const frc (active_video_frame_rate(), film->video_frame_rate());
	return DCPTime::from_frames (llrint (video->length_after_3d_combine() * frc.factor ()), film->video_frame_rate ());
}

string
ImageContent::identifier () const
{
	SafeStringStream s;
	s << Content::identifier();
	s << "_" << video->identifier ();
	s << "_" << video->length();
	return s.str ();
}

bool
ImageContent::still () const
{
	return number_of_paths() == 1;
}

void
ImageContent::set_default_colour_conversion ()
{
	BOOST_FOREACH (boost::filesystem::path i, _paths) {
		if (valid_j2k_file (i)) {
			/* We default to no colour conversion if we have JPEG2000 files */
			video->unset_colour_conversion ();
			return;
		}
	}

	bool const s = still ();

	boost::mutex::scoped_lock lm (_mutex);

	if (s) {
		video->set_colour_conversion (PresetColourConversion::from_id ("srgb").conversion);
	} else {
		video->set_colour_conversion (PresetColourConversion::from_id ("rec709").conversion);
	}
}
