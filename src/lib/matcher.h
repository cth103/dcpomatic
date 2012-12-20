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

#include <boost/optional.hpp>
#include "processor.h"

class Matcher : public AudioVideoProcessor
{
public:
	Matcher (Log* log, int sample_rate, float frames_per_second);
	void process_video (boost::shared_ptr<Image> i, bool, boost::shared_ptr<Subtitle> s);
	void process_audio (boost::shared_ptr<AudioBuffers>);
	void process_end ();

private:
	int _sample_rate;
	float _frames_per_second;
	int _video_frames;
	int64_t _audio_frames;
	boost::optional<AVPixelFormat> _pixel_format;
	boost::optional<Size> _size;
	boost::optional<int> _channels;
};
