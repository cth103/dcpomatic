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


#include "dcp_video.h"
#include "dcpomatic_log.h"
#include "j2k_encoder.h"
#include "j2k_sync_encoder_thread.h"
#include <dcp/scope_guard.h>


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

		dcp::ScopeGuard frame_guard([this, &frame]() {
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

