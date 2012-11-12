#include "audio_source.h"
#include "audio_sink.h"

using boost::shared_ptr;
using boost::bind;

void
AudioSource::connect_audio (shared_ptr<AudioSink> s)
{
	Audio.connect (bind (&AudioSink::process_audio, s, _1));
}
