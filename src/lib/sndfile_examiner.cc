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

#include "sndfile_examiner.h"

using boost::shared_ptr;

SndfileExaminer::SndfileExaminer (shared_ptr<const SndfileContent> content)
	: Sndfile (content)
{

}

int
SndfileExaminer::audio_channels () const
{
	return _info.channels;
}

Frame
SndfileExaminer::audio_length () const
{
	return _info.frames;
}

int
SndfileExaminer::audio_frame_rate () const
{
	return _info.samplerate;
}
