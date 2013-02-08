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

#include <sndfile.h>
#include "decoder.h"
#include "audio_decoder.h"
#include "stream.h"

class ExternalAudioStream : public AudioStream
{
public:
	ExternalAudioStream (int sample_rate, int64_t layout)
		: AudioStream (sample_rate, layout)
	{}
			       
	std::string to_string () const;

	static boost::shared_ptr<ExternalAudioStream> create ();
	static boost::shared_ptr<ExternalAudioStream> create (std::string t, boost::optional<int> v);

private:
	friend class stream_test;
	
	ExternalAudioStream ();
	ExternalAudioStream (std::string t, boost::optional<int> v);
};

class ExternalAudioDecoder : public AudioDecoder
{
public:
	ExternalAudioDecoder (boost::shared_ptr<Film>, DecodeOptions);

	bool pass ();

private:
	std::vector<SNDFILE*> open_files (sf_count_t &);
	void close_files (std::vector<SNDFILE*> const &);
};
