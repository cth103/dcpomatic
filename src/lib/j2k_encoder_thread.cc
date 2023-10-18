#include "dcp_video.h"
#include "dcpomatic_log.h"
#include "j2k_encoder.h"
#include "j2k_encoder_thread.h"
#include "scope_guard.h"


J2KEncoderThread::J2KEncoderThread(J2KEncoder& encoder)
	: _encoder(encoder)
{

}


void
J2KEncoderThread::start()
{
	_thread = boost::thread(boost::bind(&J2KEncoderThread::run, this));
#ifdef DCPOMATIC_LINUX
	pthread_setname_np(_thread.native_handle(), "encode-worker");
#endif
}


void
J2KEncoderThread::stop()
{
	_thread.interrupt();
	try {
		_thread.join();
	} catch (std::exception& e) {
		LOG_ERROR("join() threw an exception: %1", e.what());
	} catch (...) {
		LOG_ERROR_NC("join() threw an exception");
	}
}


