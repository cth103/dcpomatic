#include "cpu_j2k_encoder_thread.h"
#include "cross.h"
#include "dcpomatic_log.h"
#include "dcp_video.h"
#include "j2k_encoder.h"
#include "scope_guard.h"
#include "util.h"

#include "i18n.h"


using std::make_shared;
using std::shared_ptr;


CPUJ2KEncoderThread::CPUJ2KEncoderThread(J2KEncoder& encoder)
	: J2KSyncEncoderThread(encoder)
{

}


void
CPUJ2KEncoderThread::log_thread_start() const
{
	start_of_thread("CPUJ2KEncoder");
	LOG_TIMING("start-encoder-thread thread=%1 server=localhost", thread_id());
}


shared_ptr<dcp::ArrayData>
CPUJ2KEncoderThread::encode(DCPVideo const& frame)
{
	try {
		return make_shared<dcp::ArrayData>(frame.encode_locally());
	} catch (std::exception& e) {
		LOG_ERROR(N_("Local encode failed (%1)"), e.what());
	}

	return {};
}

