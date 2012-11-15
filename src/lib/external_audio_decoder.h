#include "decoder.h"
#include "audio_decoder.h"

class ExternalAudioDecoder : public AudioDecoder
{
public:
	ExternalAudioDecoder (boost::shared_ptr<Film>, boost::shared_ptr<const Options>, Job *);

	bool pass ();
};
