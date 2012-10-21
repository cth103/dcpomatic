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

#include "examine_content_job.h"
#include "options.h"
#include "film_state.h"
#include "decoder_factory.h"
#include "decoder.h"
#include "imagemagick_encoder.h"
#include "transcoder.h"

using namespace std;
using namespace boost;

ExamineContentJob::ExamineContentJob (shared_ptr<const FilmState> fs, Log* l, shared_ptr<Job> req)
	: Job (fs, l, req)
{

}

ExamineContentJob::~ExamineContentJob ()
{
}

string
ExamineContentJob::name () const
{
	if (_fs->name().empty ()) {
		return "Examine content";
	}
	
	return String::compose ("Examine content of %1", _fs->name());
}

void
ExamineContentJob::run ()
{
	shared_ptr<FilmState> fs = _fs->state_copy ();
	
	/* Decode the content to get an accurate length */
	
	shared_ptr<Options> o (new Options ("", "", ""));
	o->out_size = Size (512, 512);
	o->apply_crop = false;

	descend (0.5);

	_decoder = decoder_factory (fs, o, this, _log, true, true);
	_decoder->go ();
	fs->set_length (last_video_frame ());

	ascend ();

	/* Now make thumbnails for it */

	descend (0.5);

	try {
		o.reset (new Options (fs->dir ("thumbs"), ".png", ""));
		o->out_size = fs->size ();
		o->apply_crop = false;
		o->decode_audio = false;
		o->decode_video_frequency = 128;
		o->decode_subtitles = true;
		shared_ptr<ImageMagickEncoder> e (new ImageMagickEncoder (fs, o, _log));
		Transcoder w (fs, o, this, _log, e);
		w.go ();
		
	} catch (std::exception& e) {

		ascend ();
		set_progress (1);
		set_error (e.what ());
		set_state (FINISHED_ERROR);
		return;
		
	}

	ascend ();
	set_progress (1);
	set_state (FINISHED_OK);
}

int
ExamineContentJob::last_video_frame () const
{
	return _decoder->last_video_frame ();
}
