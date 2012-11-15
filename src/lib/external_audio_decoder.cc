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
	for (vector<string>::const_iterator i = files.begin(); i != files.end(); ++i) {
		if (i->empty ()) {
			sndfiles.push_back (0);
		} else {
			SF_INFO info;
			SNDFILE* s = sf_open (i->c_str(), SFM_READ, &info);
			if (!s) {
				throw DecodeError ("could not open external audio file for reading");
			}

			if (info.channels != 1) {
				throw DecodeError ("external audio files must be mono");
			}
			
			sndfiles.push_back (s);

			if (first) {
				/* XXX: nasty magic value */
				AudioStream st ("DVDOMATIC-EXTERNAL", -1, info.samplerate, av_get_default_channel_layout (info.channels));
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
