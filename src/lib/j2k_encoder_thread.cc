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
#include "j2k_encoder_thread.h"


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


