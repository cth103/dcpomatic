#ifndef DVDOMATIC_AUDIO_SOURCE_H
#define DVDOMATIC_AUDIO_SOURCE_H

#include <boost/signals2.hpp>

class AudioBuffers;
class AudioSink;

class AudioSource
{
public:
	/** Emitted when some audio data is ready */
	boost::signals2::signal<void (boost::shared_ptr<AudioBuffers>)> Audio;

	void connect_audio (boost::shared_ptr<AudioSink>);
};

#endif
