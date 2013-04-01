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

#include <iostream>
#include <sndfile.h>
#include "sndfile_content.h"
#include "sndfile_decoder.h"
#include "film.h"
#include "exceptions.h"

#include "i18n.h"

using std::vector;
using std::string;
using std::stringstream;
using std::min;
using std::cout;
using boost::shared_ptr;
using boost::optional;

/* XXX */

SndfileDecoder::SndfileDecoder (shared_ptr<const Film> f, shared_ptr<const SndfileContent> c)
	: Decoder (f)
	, AudioDecoder (f)
{
	sf_count_t frames;
	SNDFILE* sf = open_file (frames);
	sf_close (sf);
}

SNDFILE*
SndfileDecoder::open_file (sf_count_t & frames)
{
	frames = 0;
	
	SF_INFO info;
	SNDFILE* s = sf_open (_sndfile_content->file().string().c_str(), SFM_READ, &info);
	if (!s) {
		throw DecodeError (_("could not open external audio file for reading"));
	}

	frames = info.frames;
	return s;
}

bool
SndfileDecoder::pass ()
{
	sf_count_t frames;
	SNDFILE* sndfile = open_file (frames);

	/* Do things in half second blocks as I think there may be limits
	   to what FFmpeg (and in particular the resampler) can cope with.
	*/
	sf_count_t const block = _sndfile_content->audio_frame_rate() / 2;

	shared_ptr<AudioBuffers> audio (new AudioBuffers (_sndfile_content->audio_channels(), block));
	while (frames > 0) {
		sf_count_t const this_time = min (block, frames);
		sf_read_float (sndfile, audio->data(0), this_time);
		audio->set_frames (this_time);
		Audio (audio);
		frames -= this_time;
	}

	sf_close (sndfile);

	return true;
}
