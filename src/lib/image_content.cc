/*
    Copyright (C) 2013-2018 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "image_content.h"
#include "video_content.h"
#include "image_examiner.h"
#include "compose.hpp"
#include "film.h"
#include "job.h"
#include "frame_rate_change.h"
#include "exceptions.h"
#include "image_filename_sorter.h"
#include <libcxml/cxml.h>
#include <libxml++/libxml++.h>
#include <boost/foreach.hpp>
#include <iostream>

#include "i18n.h"

using std::string;
using std::cout;
using std::list;
using std::vector;
using boost::shared_ptr;

ImageContent::ImageContent (boost::filesystem::path p)
{
	video.reset (new VideoContent (this));

	if (boost::filesystem::is_regular_file (p) && valid_image_file (p)) {
		add_path (p);
	} else {
		_path_to_scan = p;
	}

	set_default_colour_conversion ();
}


ImageContent::ImageContent (cxml::ConstNodePtr node, int version)
	: Content (node)
{
	video = VideoContent::from_xml (this, node, version);
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
ImageContent::as_xml (xmlpp::Node* node, bool with_paths) const
{
	node->add_child("Type")->add_child_text ("Image");
	Content::as_xml (node, with_paths);

	if (video) {
		video->as_xml (node);
	}
}

void
ImageContent::examine (shared_ptr<const Film> film, shared_ptr<Job> job)
{
	if (_path_to_scan) {
		job->sub (_("Scanning image files"));
		vector<boost::filesystem::path> paths;
		int n = 0;
		for (boost::filesystem::directory_iterator i(*_path_to_scan); i != boost::filesystem::directory_iterator(); ++i) {
			if (boost::filesystem::is_regular_file (i->path()) && valid_image_file (i->path())) {
				paths.push_back (i->path());
			}
			++n;
			if ((n % 1000) == 0) {
				job->set_progress_unknown ();
			}
		}

		if (paths.empty()) {
			throw FileError (_("No valid image files were found in the folder."), *_path_to_scan);
		}

		sort (paths.begin(), paths.end(), ImageFilenameSorter());
		set_paths (paths);
	}

	Content::examine (film, job);

	shared_ptr<ImageExaminer> examiner (new ImageExaminer (film, shared_from_this(), job));
	video->take_from_examiner (examiner);
	set_default_colour_conversion ();
}

DCPTime
ImageContent::full_length (shared_ptr<const Film> film) const
{
	FrameRateChange const frc (film, shared_from_this());
	return DCPTime::from_frames (llrint(video->length_after_3d_combine() * frc.factor()), film->video_frame_rate());
}

DCPTime
ImageContent::approximate_length () const
{
	return DCPTime::from_frames (video->length_after_3d_combine(), 24);
}

string
ImageContent::identifier () const
{
	char buffer[256];
	snprintf (buffer, sizeof(buffer), "%s_%s_%" PRId64, Content::identifier().c_str(), video->identifier().c_str(), video->length());
	return buffer;
}

bool
ImageContent::still () const
{
	return number_of_paths() == 1;
}

void
ImageContent::set_default_colour_conversion ()
{
	BOOST_FOREACH (boost::filesystem::path i, paths()) {
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

void
ImageContent::add_properties (shared_ptr<const Film> film, list<UserProperty>& p) const
{
	Content::add_properties (film, p);
	video->add_properties (p);
}
