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

#include "audio_decoder.h"
#include "audio_buffers.h"
#include "exceptions.h"
#include "log.h"

#include "i18n.h"

using std::stringstream;
using std::list;
using std::pair;
using std::cout;
using boost::optional;
using boost::shared_ptr;

AudioDecoder::AudioDecoder (shared_ptr<const Film> f)
	: Decoder (f)
	, _audio_position (0)
{
}

void
AudioDecoder::audio (shared_ptr<const AudioBuffers> data, AudioContent::Frame frame)
{
	Audio (data, frame);
	_audio_position = frame + data->frames ();
}
