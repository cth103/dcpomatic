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

#ifndef DVDOMATIC_AUDIO_DECODER_H
#define DVDOMATIC_AUDIO_DECODER_H

#include "audio_source.h"
#include "stream.h"
#include "decoder.h"

class AudioDecoder : public AudioSource, public virtual Decoder
{
public:
	AudioDecoder (boost::shared_ptr<Film>, boost::shared_ptr<const Options>, Job *);

	virtual void set_audio_stream (boost::shared_ptr<AudioStream>);

	boost::shared_ptr<AudioStream> audio_stream () const {
		return _audio_stream;
	}

	std::vector<boost::shared_ptr<AudioStream> > audio_streams () const {
		return _audio_streams;
	}

protected:
	boost::shared_ptr<AudioStream> _audio_stream;
	std::vector<boost::shared_ptr<AudioStream> > _audio_streams;
};

#endif
