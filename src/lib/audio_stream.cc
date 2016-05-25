/*
    Copyright (C) 2015-2016 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "audio_stream.h"
#include "audio_mapping.h"
#include "util.h"

AudioStream::AudioStream (int frame_rate, Frame length, int channels)
	: _frame_rate (frame_rate)
	, _length (length)
{
	_mapping = AudioMapping (channels, MAX_DCP_AUDIO_CHANNELS);
}

AudioStream::AudioStream (int frame_rate, Frame length, AudioMapping mapping)
	: _frame_rate (frame_rate)
	, _length (length)
	, _mapping (mapping)
{

}

void
AudioStream::set_mapping (AudioMapping mapping)
{
	boost::mutex::scoped_lock lm (_mutex);
	_mapping = mapping;
}

int
AudioStream::channels () const
{
	boost::mutex::scoped_lock lm (_mutex);
	return _mapping.input_channels ();
}
