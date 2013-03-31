#include "sndfile_content.h"
#include "compose.hpp"

#include "i18n.h"

using namespace std;

SndfileContent::SndfileContent (boost::filesystem::path f)
	: Content (f)
	, AudioContent (f)
{

}

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
	

bool
SndfileContent::valid_file (boost::filesystem::path f)
{
	/* XXX: more extensions */
	string ext = f.extension().string();
	transform (ext.begin(), ext.end(), ext.begin(), ::tolower);
	return (ext == ".wav" || ext == ".aif" || ext == ".aiff");
}
