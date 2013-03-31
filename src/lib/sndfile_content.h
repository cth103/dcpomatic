#include "audio_content.h"

class SndfileContent : public AudioContent
{
public:
	std::string summary () const;

        /* AudioDecoder */
        int audio_channels () const;
        ContentAudioFrame audio_length () const;
        int audio_frame_rate () const;
        int64_t audio_channel_layout () const;
};
