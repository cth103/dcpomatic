#ifndef DVDOMATIC_PROCESSOR_H
#define DVDOMATIC_PROCESSOR_H

#include "video_source.h"
#include "video_sink.h"
#include "audio_source.h"
#include "audio_sink.h"

class Log;

class Processor
{
public:
	Processor (Log* log)
		: _log (log)
	{}
	
	virtual void process_begin () {}
	virtual void process_end () {}

protected:
	Log* _log;
};

class AudioVideoProcessor : public Processor, public VideoSource, public VideoSink, public AudioSource, public AudioSink
{
public:
	AudioVideoProcessor (Log* log)
		: Processor (log)
	{}
};

class AudioProcessor : public Processor, public AudioSource, public AudioSink
{
public:
	AudioProcessor (Log* log)
		: Processor (log)
	{}
};

class VideoProcessor : public Processor, public VideoSource, public VideoSink
{
public:
	VideoProcessor (Log* log)
		: Processor (log)
	{}
};

#endif
