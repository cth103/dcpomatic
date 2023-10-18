#include "config.h"
#include "cross.h"
#include "dcpomatic_log.h"
#include "dcp_video.h"
#include "grok/context.h"
#include "grok_j2k_encoder_thread.h"
#include "j2k_encoder.h"
#include "scope_guard.h"
#include "util.h"

#include "i18n.h"


using std::make_shared;
using std::shared_ptr;


GrokJ2KEncoderThread::GrokJ2KEncoderThread(J2KEncoder& encoder, grk_plugin::GrokContext* context)
	: J2KEncoderThread(encoder)
	, _context(context)
{

}


void
GrokJ2KEncoderThread::run()
try
{
	while (true)
	{
		LOG_TIMING("encoder-sleep thread=%1", thread_id());
		auto frame = _encoder.pop();

		ScopeGuard frame_guard([this, &frame]() {
			LOG_ERROR("Failed to schedule encode of %1 using grok", frame.index());
			_encoder.retry(frame);
		});

		LOG_TIMING("encoder-pop thread=%1 frame=%2 eyes=%3", thread_id(), frame.index(), static_cast<int>(frame.eyes()));

		auto grok = Config::instance()->grok().get_value_or({});

		if (_context->launch(frame, grok.selected) && _context->scheduleCompress(frame)) {
			frame_guard.cancel();
		}
	}
} catch (boost::thread_interrupted& e) {
} catch (...) {
	store_current();
}
