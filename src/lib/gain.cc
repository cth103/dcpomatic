#include "gain.h"

using boost::shared_ptr;

/** @param gain gain in dB */
Gain::Gain (Log* log, float gain)
	: AudioProcessor (log)
	, _gain (gain)
{

}

void
Gain::process_audio (shared_ptr<AudioBuffers> b)
{
	if (_gain != 0) {
		float const linear_gain = pow (10, _gain / 20);
		for (int i = 0; i < b->channels(); ++i) {
			for (int j = 0; j < b->frames(); ++j) {
				b->data(i)[j] *= linear_gain;
			}
		}
	}

	Audio (b);
}
