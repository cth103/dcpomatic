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

#include "decoder.h"

namespace Magick {
	class Image;
}

class ImageMagickDecoder : public Decoder
{
public:
	ImageMagickDecoder (boost::shared_ptr<Film>, boost::shared_ptr<const Options>, Job *, bool);

	float frames_per_second () const {
		return static_frames_per_second ();
	}

	Size native_size () const;

	int audio_channels () const {
		return 0;
	}

	int audio_sample_rate () const {
		return 0;
	}

	AVSampleFormat audio_sample_format () const {
		return AV_SAMPLE_FMT_NONE;
	}

	int64_t audio_channel_layout () const {
		return 0;
	}

	bool has_subtitles () const {
		return false;
	}

	static float static_frames_per_second () {
		return 24;
	}

protected:
	bool pass ();
	PixelFormat pixel_format () const;

	int time_base_numerator () const {
		return 0;
	}

	int time_base_denominator () const {
		return 0;
	}

	int sample_aspect_ratio_numerator () const {
		/* XXX */
		return 1;
	}

	int sample_aspect_ratio_denominator () const {
		/* XXX */
		return 1;
	}

private:
	Magick::Image* _magick_image;
	bool _done;
};
