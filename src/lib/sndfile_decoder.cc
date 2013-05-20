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

#include <iostream>
#include <sndfile.h>
#include "sndfile_decoder.h"
#include "film.h"
#include "exceptions.h"

#include "i18n.h"

using std::vector;
using std::string;
using std::stringstream;
using std::min;
using std::cout;
using boost::shared_ptr;
using boost::optional;

SndfileDecoder::SndfileDecoder (shared_ptr<Film> f, DecodeOptions o)
	: Decoder (f, o)
	, AudioDecoder (f, o)
	, _done (0)
	, _frames (0)
{
	_done = 0;
	_frames = 0;
	
	vector<string> const files = _film->external_audio ();

	int N = 0;
	for (size_t i = 0; i < files.size(); ++i) {
		if (!files[i].empty()) {
			N = i + 1;
		}
	}

	if (N == 0) {
		return;
	}

	bool first = true;
	
	for (size_t i = 0; i < (size_t) N; ++i) {
		if (files[i].empty ()) {
			_sndfiles.push_back (0);
		} else {
			SF_INFO info;
			SNDFILE* s = sf_open (files[i].c_str(), SFM_READ, &info);
			if (!s) {
				throw DecodeError (_("could not open external audio file for reading"));
			}

			if (info.channels != 1) {
				throw DecodeError (_("external audio files must be mono"));
			}
			
			_sndfiles.push_back (s);

			if (first) {
				shared_ptr<SndfileStream> st (
					new SndfileStream (
						info.samplerate, av_get_default_channel_layout (N)
						)
					);
				
				_audio_streams.push_back (st);
				_audio_stream = st;
				_frames = info.frames;
				first = false;
			} else {
				if (info.frames != _frames) {
					throw DecodeError (_("external audio files have differing lengths"));
				}
			}
		}
	}
}

bool
SndfileDecoder::pass ()
{
	if (_audio_streams.empty ()) {
		return true;
	}
	
	/* Do things in half second blocks as I think there may be limits
	   to what FFmpeg (and in particular the resampler) can cope with.
	*/
	sf_count_t const block = _audio_stream->sample_rate() / 2;
	shared_ptr<AudioBuffers> audio (new AudioBuffers (_audio_stream->channels(), block));
	sf_count_t const this_time = min (block, _frames - _done);
	for (size_t i = 0; i < _sndfiles.size(); ++i) {
		if (!_sndfiles[i]) {
			audio->make_silent (i);
		} else {
			sf_read_float (_sndfiles[i], audio->data(i), this_time);
		}
	}

	audio->set_frames (this_time);
	Audio (audio, double(_done) / _audio_stream->sample_rate());
	_done += this_time;

	return (_done == _frames);
}

SndfileDecoder::~SndfileDecoder ()
{
	for (size_t i = 0; i < _sndfiles.size(); ++i) {
		if (_sndfiles[i]) {
			sf_close (_sndfiles[i]);
		}
	}
}

shared_ptr<SndfileStream>
SndfileStream::create ()
{
	return shared_ptr<SndfileStream> (new SndfileStream);
}

shared_ptr<SndfileStream>
SndfileStream::create (string t, optional<int> v)
{
	if (!v) {
		/* version < 1; no type in the string, and there's only FFmpeg streams anyway */
		return shared_ptr<SndfileStream> ();
	}

	stringstream s (t);
	string type;
	s >> type;
	if (type != N_("external")) {
		return shared_ptr<SndfileStream> ();
	}

	return shared_ptr<SndfileStream> (new SndfileStream (t, v));
}

SndfileStream::SndfileStream (string t, optional<int> v)
{
	assert (v);

	stringstream s (t);
	string type;
	s >> type >> _sample_rate >> _channel_layout;
}

SndfileStream::SndfileStream ()
{

}

string
SndfileStream::to_string () const
{
	return String::compose (N_("external %1 %2"), _sample_rate, _channel_layout);
}
