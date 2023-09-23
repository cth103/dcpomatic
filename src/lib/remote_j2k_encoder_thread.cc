#include "dcp_video.h"
#include "dcpomatic_log.h"
#include "j2k_encoder.h"
#include "remote_j2k_encoder_thread.h"
#include "scope_guard.h"
#include "util.h"

#include "i18n.h"


using std::make_shared;
using std::shared_ptr;


RemoteJ2KEncoderThread::RemoteJ2KEncoderThread(J2KEncoder& encoder, EncodeServerDescription server)
	: J2KSyncEncoderThread(encoder)
	, _server(server)
{

}


void
RemoteJ2KEncoderThread::log_thread_start() const
{
	start_of_thread("RemoteJ2KEncoder");
	LOG_TIMING("start-encoder-thread thread=%1 server=%2", thread_id(), _server.host_name());
}


shared_ptr<dcp::ArrayData>
RemoteJ2KEncoderThread::encode(DCPVideo const& frame)
{
	shared_ptr<dcp::ArrayData> encoded;

	try {
		encoded = make_shared<dcp::ArrayData>(frame.encode_remotely(_server));
		if (_remote_backoff > 0) {
			LOG_GENERAL("%1 was lost, but now she is found; removing backoff", _server.host_name());
			_remote_backoff = 0;
		}
	} catch (std::exception& e) {
		LOG_ERROR(
			N_("Remote encode of %1 on %2 failed (%3); thread sleeping for %4s"),
			frame.index(), _server.host_name(), e.what(), _remote_backoff
		);
	} catch (...) {
		LOG_ERROR(
			N_("Remote encode of %1 on %2 failed; thread sleeping for %4s"),
			frame.index(), _server.host_name(), _remote_backoff
		);
	}

	if (!encoded) {
		if (_remote_backoff < 60) {
			/* back off more */
			_remote_backoff += 10;
		}
		boost::this_thread::sleep(boost::posix_time::seconds(_remote_backoff));
	}

	return encoded;
}

