#ifndef DVDOMATIC_AUDIO_CONTENT_H
#define DVDOMATIC_AUDIO_CONTENT_H

#include "content.h"
#include "util.h"

class AudioContent : public virtual Content
{
public:
	AudioContent (boost::filesystem::path);

        virtual int audio_channels () const = 0;
        virtual ContentAudioFrame audio_length () const = 0;
        virtual int audio_frame_rate () const = 0;
        virtual int64_t audio_channel_layout () const = 0;
	
};

#endif
