#include "audio_decoder.h"
#include "stream.h"

using boost::optional;
using boost::shared_ptr;

AudioDecoder::AudioDecoder (shared_ptr<Film> f, shared_ptr<const Options> o, Job* j)
	: Decoder (f, o, j)
{

}

void
AudioDecoder::set_audio_stream (optional<AudioStream> s)
{
	_audio_stream = s;
}
