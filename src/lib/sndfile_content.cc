#include "sndfile_content.h"
#include "compose.hpp"

#include "i18n.h"

using namespace std;

string
SndfileContent::summary () const
{
	return String::compose (_("Sound file: %1"), file().filename ());
}

int
SndfileContent::audio_channels () const
{
	/* XXX */
	return 0;
}

ContentAudioFrame
SndfileContent::audio_length () const
{
	/* XXX */
	return 0;
}

int
SndfileContent::audio_frame_rate () const
{
	/* XXX */
	return 0;
}

int64_t
SndfileContent::audio_channel_layout () const
{
	/* XXX */
	return 0;
}
	
