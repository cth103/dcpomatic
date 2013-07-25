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
#include "moving_image_content.h"
#include "moving_image_examiner.h"
#include "config.h"
#include "compose.hpp"
#include "film.h"

#include "i18n.h"

using std::string;
using std::cout;
using std::list;
using std::stringstream;
using std::vector;
using boost::shared_ptr;

MovingImageContent::MovingImageContent (shared_ptr<const Film> f, boost::filesystem::path p)
	: Content (f, p)
	, VideoContent (f, p)
{

}

MovingImageContent::MovingImageContent (shared_ptr<const Film> f, shared_ptr<const cxml::Node> node)
	: Content (f, node)
	, VideoContent (f, node)
{
	list<shared_ptr<cxml::Node> > c = node->node_children ("File");
	for (list<shared_ptr<cxml::Node> >::const_iterator i = c.begin(); i != c.end(); ++i) {
		_files.push_back ((*i)->content ());
	}
}

string
MovingImageContent::summary () const
{
	/* Get the string() here so that the name does not have quotes around it */
	return String::compose (_("%1 [moving images]"), path().filename().string());
}

string
MovingImageContent::technical_summary () const
{
	return Content::technical_summary() + " - "
		+ VideoContent::technical_summary() + " - "
		+ "moving";
}

void
MovingImageContent::as_xml (xmlpp::Node* node) const
{
	node->add_child("Type")->add_child_text ("MovingImage");
	Content::as_xml (node);
	VideoContent::as_xml (node);

	for (vector<boost::filesystem::path>::const_iterator i = _files.begin(); i != _files.end(); ++i) {
		node->add_child("File")->add_child_text (i->filename().string());
	}
}

void
MovingImageContent::examine (shared_ptr<Job> job)
{
	Content::examine (job);

	shared_ptr<const Film> film = _film.lock ();
	assert (film);
	
	shared_ptr<MovingImageExaminer> examiner (new MovingImageExaminer (film, shared_from_this(), job));

	take_from_video_examiner (examiner);

	_video_length = examiner->files().size ();
	_files = examiner->files ();
}

Time
MovingImageContent::length () const
{
	shared_ptr<const Film> film = _film.lock ();
	assert (film);

	FrameRateConversion frc (video_frame_rate(), film->video_frame_rate ());
	return video_length() * frc.factor() * TIME_HZ / video_frame_rate();
}

string
MovingImageContent::identifier () const
{
	stringstream s;
	s << VideoContent::identifier ();
	s << "_" << video_length();
	return s.str ();
}
