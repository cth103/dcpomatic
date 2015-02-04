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

#include "dcp_content.h"
#include "dcp_examiner.h"
#include "job.h"
#include "film.h"
#include "config.h"
#include "compose.hpp"
#include <dcp/dcp.h>
#include <dcp/exceptions.h>
#include <iterator>

#include "i18n.h"

using std::string;
using std::cout;
using std::distance;
using boost::shared_ptr;
using boost::optional;

int const DCPContentProperty::CAN_BE_PLAYED = 600;

DCPContent::DCPContent (shared_ptr<const Film> f, boost::filesystem::path p)
	: Content (f)
	, VideoContent (f)
	, SingleStreamAudioContent (f)
	, SubtitleContent (f)
	, _has_subtitles (false)
	, _encrypted (false)
	, _kdm_valid (false)
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
	_has_subtitles = node->bool_child ("HasSubtitles");
	_encrypted = node->bool_child ("Encrypted");
	if (node->optional_node_child ("KDM")) {
		_kdm = dcp::EncryptedKDM (node->string_child ("KDM"));
	}
	_kdm_valid = node->bool_child ("KDMValid");
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
	bool const could_be_played = can_be_played ();
		
	job->set_progress_unknown ();
	Content::examine (job);
	
	shared_ptr<DCPExaminer> examiner (new DCPExaminer (shared_from_this ()));
	take_from_video_examiner (examiner);
	take_from_audio_examiner (examiner);

	boost::mutex::scoped_lock lm (_mutex);
	_name = examiner->name ();
	_has_subtitles = examiner->has_subtitles ();
	_encrypted = examiner->encrypted ();
	_kdm_valid = examiner->kdm_valid ();

	if (could_be_played != can_be_played ()) {
		signal_changed (DCPContentProperty::CAN_BE_PLAYED);
	}
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

	boost::mutex::scoped_lock lm (_mutex);
	node->add_child("Name")->add_child_text (_name);
	node->add_child("HasSubtitles")->add_child_text (_has_subtitles ? "1" : "0");
	node->add_child("Encrypted")->add_child_text (_encrypted ? "1" : "0");
	if (_kdm) {
		node->add_child("KDM")->add_child_text (_kdm->as_xml ());
	}
	node->add_child("KDMValid")->add_child_text (_kdm_valid ? "1" : "0");
}

DCPTime
DCPContent::full_length () const
{
	shared_ptr<const Film> film = _film.lock ();
	DCPOMATIC_ASSERT (film);
	return DCPTime (video_length (), FrameRateChange (video_frame_rate (), film->video_frame_rate ()));
}

string
DCPContent::identifier () const
{
	return SubtitleContent::identifier ();
}

void
DCPContent::add_kdm (dcp::EncryptedKDM k)
{
	_kdm = k;
}

bool
DCPContent::can_be_played () const
{
	return !_encrypted || _kdm_valid;
}

boost::filesystem::path
DCPContent::directory () const
{
	optional<size_t> smallest;
	boost::filesystem::path dir;
	for (size_t i = 0; i < number_of_paths(); ++i) {
		boost::filesystem::path const p = path (i).parent_path ();
		size_t const d = distance (p.begin(), p.end());
		if (!smallest || d < smallest.get ()) {
			dir = p;
		}
	}

	return dir;
}
