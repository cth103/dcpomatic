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
#include "external_audio_decoder.h"
#include "film.h"
#include "exceptions.h"

using std::vector;
using std::string;
using std::min;
using boost::shared_ptr;

ExternalAudioDecoder::ExternalAudioDecoder (shared_ptr<Film> f, shared_ptr<const Options> o, Job* j)
	: Decoder (f, o, j)
	, AudioDecoder (f, o, j)
{

}

bool
ExternalAudioDecoder::pass ()
{
	vector<string> const files = _film->external_audio ();

	int N = 0;
	for (size_t i = 0; i < files.size(); ++i) {
		if (!files[i].empty()) {
			N = i + 1;
		}
	}

	if (N == 0) {
		return true;
	}

	bool first = true;
	sf_count_t frames = 0;
	
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
				/* XXX: nasty magic value */
				AudioStream st ("DVDOMATIC-EXTERNAL", -1, info.samplerate, av_get_default_channel_layout (N));
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

	sf_count_t const block = 65536;

	shared_ptr<AudioBuffers> audio (new AudioBuffers (_audio_stream.get().channels(), block));
	while (frames > 0) {
		sf_count_t const this_time = min (block, frames);
		for (size_t i = 0; i < sndfiles.size(); ++i) {
			if (!sndfiles[i]) {
				audio->make_silent (i);
			} else {
				sf_read_float (sndfiles[i], audio->data(i), block);
			}
		}

		Audio (audio);
		frames -= this_time;
	}
	
	return true;
}
