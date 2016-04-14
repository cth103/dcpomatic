/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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
#include "audio_content.h"
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
using std::vector;
using boost::shared_ptr;

SndfileContent::SndfileContent (shared_ptr<const Film> film, boost::filesystem::path p)
	: Content (film, p)
{
	audio.reset (new AudioContent (this, film));
}

SndfileContent::SndfileContent (shared_ptr<const Film> film, cxml::ConstNodePtr node, int version)
	: Content (film, node)
	, _audio_length (node->number_child<Frame> ("AudioLength"))
{
	audio.reset (new AudioContent (this, film, node));
	audio->set_stream (
		AudioStreamPtr (
			new AudioStream (node->number_child<int> ("AudioFrameRate"), AudioMapping (node->node_child ("AudioMapping"), version)))
		);
}

void
SndfileContent::as_xml (xmlpp::Node* node) const
{
	node->add_child("Type")->add_child_text ("Sndfile");
	Content::as_xml (node);
	audio->as_xml (node);
	node->add_child("AudioFrameRate")->add_child_text (raw_convert<string> (audio->stream()->frame_rate ()));
	audio->stream()->mapping().as_xml (node->add_child("AudioMapping"));
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
		+ audio->technical_summary ()
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
	{
		boost::mutex::scoped_lock lm (_mutex);
		AudioStreamPtr as (new AudioStream (examiner->audio_frame_rate(), examiner->audio_channels ()));
		audio->set_stream (as);
		AudioMapping m = as->mapping ();
		film()->make_audio_mapping_default (m);
		as->set_mapping (m);
		_audio_length = examiner->audio_length ();
	}

	signal_changed (AudioContentProperty::STREAMS);
}

DCPTime
SndfileContent::full_length () const
{
	FrameRateChange const frc (audio->audio_video_frame_rate(), film->video_frame_rate());
	return DCPTime::from_frames (audio_length() / frc.speed_up, audio->stream()->frame_rate ());
}
