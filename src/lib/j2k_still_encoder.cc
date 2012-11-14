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

/** @file  src/j2k_still_encoder.cc
 *  @brief An encoder which writes JPEG2000 files for a single still source image.
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
#include "options.h"
#include "exceptions.h"
#include "dcp_video_frame.h"
#include "filter.h"
#include "log.h"
#include "imagemagick_decoder.h"
#include "film.h"

using std::string;
using std::pair;
using boost::shared_ptr;

J2KStillEncoder::J2KStillEncoder (shared_ptr<const Film> f, shared_ptr<const Options> o)
	: Encoder (f, o)
{
	
}

void
J2KStillEncoder::do_process_video (shared_ptr<Image> yuv, shared_ptr<Subtitle> sub)
{
	pair<string, string> const s = Filter::ffmpeg_strings (_film->filters());
	DCPVideoFrame* f = new DCPVideoFrame (
		yuv, sub, _opt->out_size, _opt->padding, _film->subtitle_offset(), _film->subtitle_scale(), _film->scaler(), 0, _film->frames_per_second(), s.second,
		Config::instance()->colour_lut_index(), Config::instance()->j2k_bandwidth(),
		_film->log()
		);

	if (!boost::filesystem::exists (_opt->frame_out_path (0, false))) {
		boost::shared_ptr<EncodedData> e = f->encode_locally ();
		e->write (_opt, 0);
	}

	string const real = _opt->frame_out_path (0, false);
	for (int i = 1; i < (_film->still_duration() * 24); ++i) {
		if (!boost::filesystem::exists (_opt->frame_out_path (i, false))) {
			string const link = _opt->frame_out_path (i, false);
#ifdef DVDOMATIC_POSIX			
			int const r = symlink (real.c_str(), link.c_str());
			if (r) {
				throw EncodeError ("could not create symlink");
			}
#endif
#ifdef DVDOMATIC_WINDOWS
			boost::filesystem::copy_file (real, link);
#endif			
		}
		frame_done ();
	}
}
