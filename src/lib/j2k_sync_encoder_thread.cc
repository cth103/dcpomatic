#include "dcp_video.h"
#include "dcpomatic_log.h"
#include "j2k_encoder.h"
#include "j2k_sync_encoder_thread.h"
#include "scope_guard.h"


J2KSyncEncoderThread::J2KSyncEncoderThread(J2KEncoder& encoder)
	: J2KEncoderThread(encoder)
{

}


void
J2KSyncEncoderThread::run()
try
{
	log_thread_start();

	while (true) {
		LOG_TIMING("encoder-sleep thread=%1", thread_id());
		auto frame = _encoder.pop();

		ScopeGuard frame_guard([this, &frame]() {
			boost::this_thread::disable_interruption dis;
			_encoder.retry(frame);
		});

		LOG_TIMING("encoder-pop thread=%1 frame=%2 eyes=%3", thread_id(), frame.index(), static_cast<int>(frame.eyes()));

		auto encoded = encode(frame);

		if (encoded) {
			boost::this_thread::disable_interruption dis;
			frame_guard.cancel();
			_encoder.write(encoded, frame.index(), frame.eyes());
		}
	}
} catch (boost::thread_interrupted& e) {
} catch (...) {
	store_current();
}

