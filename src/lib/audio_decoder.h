#ifndef DVDOMATIC_AUDIO_DECODER_H
#define DVDOMATIC_AUDIO_DECODER_H

#include "audio_source.h"
#include "stream.h"
#include "decoder.h"

class AudioDecoder : public AudioSource, public virtual Decoder
{
public:
	AudioDecoder (boost::shared_ptr<Film>, boost::shared_ptr<const Options>, Job *);

	virtual void set_audio_stream (boost::optional<AudioStream>);

	boost::optional<AudioStream> audio_stream () const {
		return _audio_stream;
	}

	std::vector<AudioStream> audio_streams () const {
		return _audio_streams;
	}

protected:
	boost::optional<AudioStream> _audio_stream;
	std::vector<AudioStream> _audio_streams;
};

#endif
