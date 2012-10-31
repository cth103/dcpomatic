/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

/** @file  src/examine_content_job.cc
 *  @brief A class to run through content at high speed to find its length.
 */

#include <boost/filesystem.hpp>
#include "examine_content_job.h"
#include "options.h"
#include "decoder_factory.h"
#include "decoder.h"
#include "imagemagick_encoder.h"
#include "transcoder.h"
#include "log.h"
#include "film.h"

using std::string;
using std::vector;
using boost::shared_ptr;

ExamineContentJob::ExamineContentJob (shared_ptr<Film> f, shared_ptr<Job> req)
	: Job (f, req)
{

}

ExamineContentJob::~ExamineContentJob ()
{
}

string
ExamineContentJob::name () const
{
	if (_film->name().empty ()) {
		return "Examine content";
	}
	
	return String::compose ("Examine content of %1", _film->name());
}

void
ExamineContentJob::run ()
{
	/* Decode the content to get an accurate length */
	
	shared_ptr<Options> o (new Options ("", "", ""));
	o->out_size = Size (512, 512);
	o->apply_crop = false;

	descend (0.5);

	_decoder = decoder_factory (_film, o, this, true, true);
	_decoder->go ();

	_film->set_length (_decoder->video_frame_index());

	_film->log()->log (String::compose ("Video length is %1 frames", _decoder->video_frame_index()));

	ascend ();

	/* Now make thumbnails for it */

	descend (0.5);

	try {
		o.reset (new Options (_film->dir ("thumbs"), ".png", ""));
		o->out_size = _film->size ();
		o->apply_crop = false;
		o->decode_audio = false;
		o->decode_video_frequency = 128;
		o->decode_subtitles = true;
		shared_ptr<ImageMagickEncoder> e (new ImageMagickEncoder (_film, o));
		Transcoder w (_film, o, this, e);
		w.go ();
		
	} catch (std::exception& e) {

		ascend ();
		set_progress (1);
		set_error (e.what ());
		set_state (FINISHED_ERROR);
		return;
		
	}

	string const tdir = _film->dir ("thumbs");
	vector<int> thumbs;

	for (boost::filesystem::directory_iterator i = boost::filesystem::directory_iterator (tdir); i != boost::filesystem::directory_iterator(); ++i) {

		/* Aah, the sweet smell of progress */
#if BOOST_FILESYSTEM_VERSION == 3		
		string const l = boost::filesystem::path(*i).leaf().generic_string();
#else
		string const l = i->leaf ();
#endif
		
		size_t const d = l.find (".png");
		size_t const t = l.find (".tmp");
		if (d != string::npos && t == string::npos) {
			thumbs.push_back (atoi (l.substr (0, d).c_str()));
		}
	}

	sort (thumbs.begin(), thumbs.end());
	_film->set_thumbs (thumbs);	

	ascend ();
	set_progress (1);
	set_state (FINISHED_OK);
}
