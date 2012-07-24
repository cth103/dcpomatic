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

/** @file  src/j2k_wav_encoder.cc
 *  @brief An encoder which writes JPEG2000 and WAV files.
 */

#include <sstream>
#include <stdexcept>
#include <iomanip>
#include <iostream>
#include <boost/filesystem.hpp>
#include <sndfile.h>
#include <openjpeg.h>
#include "j2k_still_encoder.h"
#include "config.h"
#include "film_state.h"
#include "options.h"
#include "exceptions.h"
#include "dcp_video_frame.h"
#include "filter.h"
#include "log.h"
#include "imagemagick_decoder.h"

using namespace std;
using namespace boost;

J2KStillEncoder::J2KStillEncoder (shared_ptr<const FilmState> s, shared_ptr<const Options> o, Log* l)
	: Encoder (s, o, l)
{
	
}

void
J2KStillEncoder::process_video (shared_ptr<Image> yuv, int frame)
{
	pair<string, string> const s = Filter::ffmpeg_strings (_fs->filters);
	DCPVideoFrame* f = new DCPVideoFrame (
		yuv, _opt->out_size, _opt->padding, _fs->scaler, 0, _fs->frames_per_second, s.second,
		Config::instance()->colour_lut_index(), Config::instance()->j2k_bandwidth(),
		_log
		);

	if (!boost::filesystem::exists (_opt->frame_out_path (0, false))) {
		boost::shared_ptr<EncodedData> e = f->encode_locally ();
		e->write (_opt, 0);
	}

	string const real = _opt->frame_out_path (1, false);
	for (int i = 1; i < (_fs->still_duration * ImageMagickDecoder::static_frames_per_second()); ++i) {
		if (!boost::filesystem::exists (_opt->frame_out_path (i, false))) {
			string const link = _opt->frame_out_path (i, false);
#ifdef DVDOMATIC_POSIX			
			symlink (real.c_str(), link.c_str());
#endif
#ifdef DVDOMATIC_WINDOWS
			filesystem::copy_file (real, link);
#endif			
		}
		frame_done ();
	}
}
