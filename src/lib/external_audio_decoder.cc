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
#include "external_audio_decoder.h"
#include "film.h"
#include "exceptions.h"

using std::vector;
using std::string;
using std::stringstream;
using std::min;
using std::cout;
using boost::shared_ptr;
using boost::optional;

ExternalAudioDecoder::ExternalAudioDecoder (shared_ptr<Film> f, shared_ptr<const DecodeOptions> o, Job* j)
	: Decoder (f, o, j)
	, AudioDecoder (f, o, j)
{
	sf_count_t frames;
	vector<SNDFILE*> sf = open_files (frames);
	close_files (sf);
}

vector<SNDFILE*>
ExternalAudioDecoder::open_files (sf_count_t & frames)
{
	vector<string> const files = _film->external_audio ();

	int N = 0;
	for (size_t i = 0; i < files.size(); ++i) {
		if (!files[i].empty()) {
			N = i + 1;
		}
	}

	if (N == 0) {
		return vector<SNDFILE*> ();
	}

	bool first = true;
	frames = 0;
	
	vector<SNDFILE*> sndfiles;
	for (size_t i = 0; i < (size_t) N; ++i) {
		if (files[i].empty ()) {
			sndfiles.push_back (0);
		} else {
			SF_INFO info;
			SNDFILE* s = sf_open (files[i].c_str(), SFM_READ, &info);
			if (!s) {
				throw DecodeError ("could not open external audio file for reading");
			}

			if (info.channels != 1) {
				throw DecodeError ("external audio files must be mono");
			}
			
			sndfiles.push_back (s);

			if (first) {
				shared_ptr<ExternalAudioStream> st (
					new ExternalAudioStream (
						info.samplerate, av_get_default_channel_layout (N)
						)
					);
				
				_audio_streams.push_back (st);
				_audio_stream = st;
				frames = info.frames;
				first = false;
			} else {
				if (info.frames != frames) {
					throw DecodeError ("external audio files have differing lengths");
				}
			}
		}
	}

	return sndfiles;
}

bool
ExternalAudioDecoder::pass ()
{
	sf_count_t frames;
	vector<SNDFILE*> sndfiles = open_files (frames);
	if (sndfiles.empty()) {
		return true;
	}

	/* Do things in half second blocks as I think there may be limits
	   to what FFmpeg (and in particular the resampler) can cope with.
	*/
	sf_count_t const block = _audio_stream->sample_rate() / 2;

	shared_ptr<AudioBuffers> audio (new AudioBuffers (_audio_stream->channels(), block));
	while (frames > 0) {
		sf_count_t const this_time = min (block, frames);
		for (size_t i = 0; i < sndfiles.size(); ++i) {
			if (!sndfiles[i]) {
				audio->make_silent (i);
			} else {
				sf_read_float (sndfiles[i], audio->data(i), block);
			}
		}

		audio->set_frames (this_time);
		Audio (audio);
		frames -= this_time;
	}

	close_files (sndfiles);

	return true;
}

void
ExternalAudioDecoder::close_files (vector<SNDFILE*> const & sndfiles)
{
	for (size_t i = 0; i < sndfiles.size(); ++i) {
		sf_close (sndfiles[i]);
	}
}

shared_ptr<ExternalAudioStream>
ExternalAudioStream::create ()
{
	return shared_ptr<ExternalAudioStream> (new ExternalAudioStream);
}

shared_ptr<ExternalAudioStream>
ExternalAudioStream::create (string t, optional<int> v)
{
	if (!v) {
		/* version < 1; no type in the string, and there's only FFmpeg streams anyway */
		return shared_ptr<ExternalAudioStream> ();
	}

	stringstream s (t);
	string type;
	s >> type;
	if (type != "external") {
		return shared_ptr<ExternalAudioStream> ();
	}

	return shared_ptr<ExternalAudioStream> (new ExternalAudioStream (t, v));
}

ExternalAudioStream::ExternalAudioStream (string t, optional<int> v)
{
	assert (v);

	stringstream s (t);
	string type;
	s >> type >> _sample_rate >> _channel_layout;
}

ExternalAudioStream::ExternalAudioStream ()
{

}

string
ExternalAudioStream::to_string () const
{
	return String::compose ("external %1 %2", _sample_rate, _channel_layout);
}
