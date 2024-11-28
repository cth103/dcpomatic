/*
    Copyright (C) 2023 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/


#include "config.h"
#include "cross.h"
#include "dcpomatic_log.h"
#include "dcp_video.h"
#include "grok/context.h"
#include "grok_j2k_encoder_thread.h"
#include "j2k_encoder.h"
#include "util.h"
#include <dcp/scope_guard.h>

#include "i18n.h"


using std::make_shared;
using std::shared_ptr;


GrokJ2KEncoderThread::GrokJ2KEncoderThread(J2KEncoder& encoder, grk_plugin::GrokContext* context)
	: J2KEncoderThread(encoder)
	, _context(context)
	, _errors(0)
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

		dcp::ScopeGuard frame_guard([this, &frame]() {
			LOG_ERROR("Failed to schedule encode of %1 using grok", frame.index());
			_errors++;
			_encoder.retry(frame);
		});

		LOG_TIMING("encoder-pop thread=%1 frame=%2 eyes=%3", thread_id(), frame.index(), static_cast<int>(frame.eyes()));

		auto grok = Config::instance()->grok().get_value_or({});

		if (_context->launch(frame, grok.selected) && _context->scheduleCompress(frame)) {
			frame_guard.cancel();
		}
	}
}
catch (boost::thread_interrupted& e)
{

}
catch (...) {
	store_current();
}
