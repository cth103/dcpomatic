/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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

#include "sndfile_base.h"
#include "audio_examiner.h"

class SndfileExaminer : public Sndfile, public AudioExaminer
{
public:
	SndfileExaminer (boost::shared_ptr<const SndfileContent> content);

	int audio_channels () const;
	Frame audio_length () const;
	int audio_frame_rate () const;
};
