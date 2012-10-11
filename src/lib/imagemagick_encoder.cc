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

/** @file src/imagemagick_encoder.cc
 *  @brief An encoder that writes image files using ImageMagick (and does nothing with audio).
 */

#include <stdexcept>
#include <vector>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <boost/filesystem.hpp>
#include <Magick++/Image.h>
#include "imagemagick_encoder.h"
#include "film.h"
#include "film_state.h"
#include "options.h"
#include "exceptions.h"
#include "image.h"
#include "subtitle.h"

using namespace std;
using namespace boost;

/** @param s FilmState of the film that we are encoding.
 *  @param o Options.
 *  @param l Log.
 */
ImageMagickEncoder::ImageMagickEncoder (shared_ptr<const FilmState> s, shared_ptr<const Options> o, Log* l)
	: Encoder (s, o, l)
{
	
}

void
ImageMagickEncoder::process_video (shared_ptr<Image> image, int frame, shared_ptr<Subtitle> sub)
{
	shared_ptr<Image> scaled = image->scale_and_convert_to_rgb (_opt->out_size, _opt->padding, _fs->scaler);

	string tmp_file = _opt->frame_out_path (frame, true);
	Magick::Image thumb (_opt->out_size.width, _opt->out_size.height, "RGB", MagickCore::CharPixel, scaled->data()[0]);
	thumb.magick ("PNG");
	thumb.write (tmp_file);
	filesystem::rename (tmp_file, _opt->frame_out_path (frame, false));

	if (sub) {
		float const x_scale = float (_opt->out_size.width) / _fs->size.width;
		float const y_scale = float (_opt->out_size.height) / _fs->size.height;

		string tmp_metadata_file = _opt->frame_out_path (frame, false, ".sub");
		ofstream metadata (tmp_metadata_file.c_str ());
		
		list<shared_ptr<SubtitleImage> > images = sub->images ();
		int n = 0;
		for (list<shared_ptr<SubtitleImage> >::iterator i = images.begin(); i != images.end(); ++i) {
			stringstream ext;
			ext << ".sub." << n << ".png";

			Size new_size = (*i)->image()->size ();
			new_size.width *= x_scale;
			new_size.height *= y_scale;
			shared_ptr<Image> scaled = (*i)->image()->scale (new_size, _fs->scaler);
			
			string tmp_sub_file = _opt->frame_out_path (frame, true, ext.str ());
			Magick::Image sub_thumb (scaled->size().width, scaled->size().height, "RGBA", MagickCore::CharPixel, scaled->data()[0]);
			sub_thumb.magick ("PNG");
			sub_thumb.write (tmp_sub_file);
			filesystem::rename (tmp_sub_file, _opt->frame_out_path (frame, false, ext.str ()));

			metadata << "image " << n << "\n"
				 << "x " << (*i)->position().x << "\n"
				 << "y " << (*i)->position().y << "\n";

			metadata.close ();
			filesystem::rename (tmp_metadata_file, _opt->frame_out_path (frame, false, ".sub"));
		}

	}
	
	frame_done (frame);
}
