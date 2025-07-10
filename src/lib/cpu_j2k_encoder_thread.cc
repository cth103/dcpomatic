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


#include "cpu_j2k_encoder_thread.h"
#include "cross.h"
#include "dcpomatic_log.h"
#include "dcp_video.h"
#include "j2k_encoder.h"
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
	LOG_TIMING("start-encoder-thread thread={} server=localhost", thread_id());
}


shared_ptr<dcp::ArrayData>
CPUJ2KEncoderThread::encode(DCPVideo const& frame)
{
	try {
		return make_shared<dcp::ArrayData>(frame.encode_locally());
	} catch (std::exception& e) {
		LOG_ERROR(N_("Local encode failed ({})"), e.what());
	}

	return {};
}

