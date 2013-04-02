#include "audio_content.h"

namespace cxml {
	class Node;
}

class SndfileContent : public AudioContent
{
public:
	SndfileContent (boost::filesystem::path);
	SndfileContent (boost::shared_ptr<const cxml::Node>);
	
	std::string summary () const;
	std::string information () const;
	boost::shared_ptr<Content> clone () const;

        /* AudioContent */
        int audio_channels () const;
        ContentAudioFrame audio_length () const;
        int audio_frame_rate () const;
        int64_t audio_channel_layout () const;

	static bool valid_file (boost::filesystem::path);
};
