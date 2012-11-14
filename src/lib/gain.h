#include "processor.h"

class Gain : public AudioProcessor
{
public:
	Gain (Log* log, float gain);

	void process_audio (boost::shared_ptr<AudioBuffers>);

private:
	float _gain;
};
