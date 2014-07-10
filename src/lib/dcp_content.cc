/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#include <dcp/dcp.h>
#include "dcp_content.h"
#include "dcp_examiner.h"
#include "job.h"
#include "film.h"
#include "compose.hpp"

#include "i18n.h"

using std::string;
using boost::shared_ptr;

DCPContent::DCPContent (shared_ptr<const Film> f, boost::filesystem::path p)
	: Content (f)
	, VideoContent (f)
	, SingleStreamAudioContent (f)
	, SubtitleContent (f)
	, _directory (p)
{
	read_directory (p);
}

DCPContent::DCPContent (shared_ptr<const Film> f, cxml::ConstNodePtr node, int version)
	: Content (f, node)
	, VideoContent (f, node, version)
	, SingleStreamAudioContent (f, node, version)
	, SubtitleContent (f, node, version)
{
	_name = node->string_child ("Name");
	_directory = node->string_child ("Directory");
}

void
DCPContent::read_directory (boost::filesystem::path p)
{
	for (boost::filesystem::directory_iterator i(p); i != boost::filesystem::directory_iterator(); ++i) {
		if (boost::filesystem::is_regular_file (i->path ())) {
			_paths.push_back (i->path ());
		} else if (boost::filesystem::is_directory (i->path ())) {
			read_directory (i->path ());
		}
	}
}

void
DCPContent::examine (shared_ptr<Job> job)
{
	job->set_progress_unknown ();
	Content::examine (job);
	shared_ptr<DCPExaminer> examiner (new DCPExaminer (shared_from_this ()));
	take_from_video_examiner (examiner);
	take_from_audio_examiner (examiner);

	boost::mutex::scoped_lock lm (_mutex);
	_name = examiner->name ();
}

string
DCPContent::summary () const
{
	boost::mutex::scoped_lock lm (_mutex);
	return String::compose (_("%1 [DCP]"), _name);
}

string
DCPContent::technical_summary () const
{
	return Content::technical_summary() + " - "
		+ VideoContent::technical_summary() + " - "
		+ AudioContent::technical_summary() + " - ";
}

void
DCPContent::as_xml (xmlpp::Node* node) const
{
	node->add_child("Type")->add_child_text ("DCP");

	Content::as_xml (node);
	VideoContent::as_xml (node);
	SingleStreamAudioContent::as_xml (node);
	SubtitleContent::as_xml (node);

	node->add_child("Name")->add_child_text (_name);
	node->add_child("Directory")->add_child_text (_directory.string ());
}

DCPTime
DCPContent::full_length () const
{
	shared_ptr<const Film> film = _film.lock ();
	assert (film);
	return DCPTime (video_length (), FrameRateChange (video_frame_rate (), film->video_frame_rate ()));
}
