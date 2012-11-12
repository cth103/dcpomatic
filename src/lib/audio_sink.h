#ifndef DVDOMATIC_AUDIO_SINK_H
#define DVDOMATIC_AUDIO_SINK_H

class AudioSink
{
public:
	/** Call with some audio data */
	virtual void process_audio (boost::shared_ptr<AudioBuffers>) = 0;
};

#endif
