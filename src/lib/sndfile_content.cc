/*
    Copyright (C) 2013-2014 Carl Hetherington <cth@carlh.net>

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
#include <dcp/raw_convert.h>
#include "sndfile_content.h"
#include "sndfile_decoder.h"
#include "film.h"
#include "compose.hpp"
#include "job.h"
#include "util.h"
#include "safe_stringstream.h"

#include "i18n.h"

using std::string;
using std::cout;
using boost::shared_ptr;
using dcp::raw_convert;

SndfileContent::SndfileContent (shared_ptr<const Film> f, boost::filesystem::path p)
	: Content (f, p)
	, SingleStreamAudioContent (f, p)
{

}

SndfileContent::SndfileContent (shared_ptr<const Film> f, cxml::ConstNodePtr node, int version)
	: Content (f, node)
	, SingleStreamAudioContent (f, node, version)
{
	
}

void
SndfileContent::as_xml (xmlpp::Node* node) const
{
	node->add_child("Type")->add_child_text ("Sndfile");
	Content::as_xml (node);
	SingleStreamAudioContent::as_xml (node);
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

string
SndfileContent::information () const
{
	if (_audio_frame_rate == 0) {
		return "";
	}
	
	SafeStringStream s;

	s << String::compose (
		_("%1 channels, %2kHz, %3 samples"),
		audio_channels(),
		audio_frame_rate() / 1000.0,
		audio_length().frames (audio_frame_rate ())
		);
	
	return s.str ();
}

bool
SndfileContent::valid_file (boost::filesystem::path f)
{
	/* XXX: more extensions */
	string ext = f.extension().string();
	transform (ext.begin(), ext.end(), ext.begin(), ::tolower);
	return (ext == ".wav" || ext == ".aif" || ext == ".aiff");
}

void
SndfileContent::examine (shared_ptr<Job> job, bool calculate_digest)
{
	job->set_progress_unknown ();
	Content::examine (job, calculate_digest);
	shared_ptr<AudioExaminer> dec (new SndfileDecoder (shared_from_this()));
	take_from_audio_examiner (dec);
}

DCPTime
SndfileContent::full_length () const
{
	shared_ptr<const Film> film = _film.lock ();
	assert (film);
	return DCPTime (audio_length(), film->active_frame_rate_change (position ()));
}

