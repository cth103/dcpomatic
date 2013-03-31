#include "audio_content.h"

class SndfileContent : public AudioContent
{
public:
	SndfileContent (boost::filesystem::path);
	
	std::string summary () const;

        /* AudioContent */
        int audio_channels () const;
        ContentAudioFrame audio_length () const;
        int audio_frame_rate () const;
        int64_t audio_channel_layout () const;

	static bool valid_file (boost::filesystem::path);
};
