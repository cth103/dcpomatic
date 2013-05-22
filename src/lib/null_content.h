#include <string>
#include <boost/shared_ptr.hpp>
#include "content.h"

class NullContent : public VideoContent, public AudioContent
{
public:
	NullContent (Time s, Time len)
		: Content (s)
		, VideoContent (s)
		, AudioContent (s)
		, _length (len)
	{}

	std::string summary () const {
		return "";
	}
	
	std::string information () const {
		return "";
	}
	
	boost::shared_ptr<Content> clone () const {
		return shared_ptr<Content> ();
	}

        int audio_channels () const {
		return MAX_AUDIO_CHANNELS;
	}
	
        ContentAudioFrame audio_length () const {
		return _length * content_audio_frame_rate() / TIME_HZ;
	}
	
        int content_audio_frame_rate () const {
		return 48000;
	}
	
	int output_audio_frame_rate (boost::shared_ptr<const Film>) const {
		return _film->dcp_audio_frame_rate (content_audio_frame_rate ());
	}
	
	AudioMapping audio_mapping () const {
		return AudioMapping ();
	}
	
	Time length (boost::shared_ptr<const Film>) const {
		return _length;
	}

private:
	Time _length;
};
