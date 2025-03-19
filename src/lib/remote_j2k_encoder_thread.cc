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
#include "remote_j2k_encoder_thread.h"
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
			N_("Remote encode of %1 on %2 failed (%3)"),
			frame.index(), _server.host_name(), e.what(), _remote_backoff
		);
	} catch (...) {
		LOG_ERROR(
			N_("Remote encode of %1 on %2 failed"),
			frame.index(), _server.host_name(), _remote_backoff
		);
	}

	if (!encoded && _remote_backoff < 60) {
		_remote_backoff += 10;
	}

	return encoded;
}

