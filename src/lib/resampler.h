#include <boost/shared_ptr.hpp>
extern "C" {
#include <libswresample/swresample.h>
}

class AudioBuffers;

class Resampler
{
public:
	Resampler (int, int, int);
	~Resampler ();

	boost::shared_ptr<const AudioBuffers> run (boost::shared_ptr<const AudioBuffers>);
	boost::shared_ptr<const AudioBuffers> flush ();

private:	
	SwrContext* _swr_context;
	int _in_rate;
	int _out_rate;
	int _channels;
};
