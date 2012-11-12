#include <boost/optional.hpp>
#include "processor.h"

class Matcher : public AudioVideoProcessor
{
public:
	Matcher (Log* log, int sample_rate, float frames_per_second);
	void process_video (boost::shared_ptr<Image> i, boost::shared_ptr<Subtitle> s);
	void process_audio (boost::shared_ptr<AudioBuffers>);
	void process_end ();

private:
	int _sample_rate;
	float _frames_per_second;
	int _video_frames;
	int64_t _audio_frames;
	boost::optional<AVPixelFormat> _pixel_format;
	boost::optional<Size> _size;
	boost::optional<int> _channels;
};
