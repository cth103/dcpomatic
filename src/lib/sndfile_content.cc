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

#include "sndfile_content.h"
#include "sndfile_decoder.h"
#include "sndfile_examiner.h"
#include "film.h"
#include "compose.hpp"
#include "job.h"
#include "util.h"
#include "safe_stringstream.h"
#include "raw_convert.h"
#include <libcxml/cxml.h>
#include <libxml++/libxml++.h>
#include <iostream>

#include "i18n.h"

using std::string;
using std::cout;
using boost::shared_ptr;

SndfileContent::SndfileContent (shared_ptr<const Film> film, boost::filesystem::path p)
	: Content (film, p)
	, SingleStreamAudioContent (film, p)
{

}

SndfileContent::SndfileContent (shared_ptr<const Film> film, cxml::ConstNodePtr node, int version)
	: Content (film, node)
	, SingleStreamAudioContent (film, node, version)
	, _audio_length (node->number_child<Frame> ("AudioLength"))
{

}

void
SndfileContent::as_xml (xmlpp::Node* node) const
{
	node->add_child("Type")->add_child_text ("Sndfile");
	Content::as_xml (node);
	SingleStreamAudioContent::as_xml (node);
	node->add_child("AudioLength")->add_child_text (raw_convert<string> (audio_length ()));
}


string
SndfileContent::summary () const
{
	/* Get the string() here so that the name does not have quotes around it */
	return String::compose (_("%1 [audio]"), path_summary ());
}

string
SndfileContent::technical_summary () const
{
	return Content::technical_summary() + " - "
		+ AudioContent::technical_summary ()
		+ " - sndfile";
}

bool
SndfileContent::valid_file (boost::filesystem::path f)
{
	/* XXX: more extensions */
	string ext = f.extension().string();
	transform (ext.begin(), ext.end(), ext.begin(), ::tolower);
	return (ext == ".wav" || ext == ".w64" || ext == ".flac" || ext == ".aif" || ext == ".aiff");
}

void
SndfileContent::examine (shared_ptr<Job> job)
{
	job->set_progress_unknown ();
	Content::examine (job);
	shared_ptr<AudioExaminer> dec (new SndfileExaminer (shared_from_this ()));
	take_from_audio_examiner (dec);
}

void
SndfileContent::take_from_audio_examiner (shared_ptr<AudioExaminer> examiner)
{
	SingleStreamAudioContent::take_from_audio_examiner (examiner);

	boost::mutex::scoped_lock lm (_mutex);
	_audio_length = examiner->audio_length ();
}

DCPTime
SndfileContent::full_length () const
{
	FrameRateChange const frc = film()->active_frame_rate_change (position ());
	return DCPTime::from_frames (audio_length() / frc.speed_up, audio_stream()->frame_rate ());
}
