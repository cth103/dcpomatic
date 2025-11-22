/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_J2K_ENCODER_H
#define DCPOMATIC_J2K_ENCODER_H


/** @file  src/j2k_encoder.h
 *  @brief J2KEncoder class.
 */


#include "cross.h"
#include "enum_indexed_vector.h"
#include "event_history.h"
#include "exception_store.h"
#include "j2k_encoder_thread.h"
#include "writer.h"
#include "video_encoder.h"
#include <dcp/warnings.h>
#include <boost/optional.hpp>
LIBDCP_DISABLE_WARNINGS
#include <boost/signals2.hpp>
LIBDCP_ENABLE_WARNINGS
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <list>
#include <stdint.h>


class DCPVideo;
class EncodeServerDescription;
class Film;
class Job;
class PlayerVideo;

namespace grk_plugin {
	struct DcpomaticContext;
	struct GrokContext;
}

struct local_threads_created_and_destroyed;
struct remote_threads_created_and_destroyed;
struct frames_not_lost_when_threads_disappear;


/** @class J2KEncoder
 *  @brief Class to manage encoding to J2K.
 *
 *  This class keeps a queue of frames to be encoded and distributes
 *  the work around threads and encoding servers.
 */
class J2KEncoder : public VideoEncoder, public ExceptionStore
{
public:
	J2KEncoder(std::shared_ptr<const Film> film, Writer& writer);
	~J2KEncoder();

	J2KEncoder(J2KEncoder const&) = delete;
	J2KEncoder& operator=(J2KEncoder const&) = delete;

	/** Called to indicate that a processing run is about to begin */
	void begin() override;

	/** Called to pass a video frame to be encoded as the next DCP frame */
	void encode(std::shared_ptr<PlayerVideo> pv, dcpomatic::DCPTime time) override;

	void pause() override;
	void resume() override;

	/** Called when a processing run has finished */
	void end() override;

	DCPVideo pop();
	void retry(DCPVideo frame);
	void write(std::shared_ptr<const dcp::Data> data, int index, Eyes eyes);

private:
	friend struct ::local_threads_created_and_destroyed;
	friend struct ::remote_threads_created_and_destroyed;
	friend struct ::frames_not_lost_when_threads_disappear;

	void frame_done();
	void servers_list_changed();
	void remake_threads(int cpu, int gpu, std::list<EncodeServerDescription> servers);
	void terminate_threads();

	boost::mutex _threads_mutex;
	std::vector<std::shared_ptr<J2KEncoderThread>> _threads;

	mutable boost::mutex _queue_mutex;
	std::list<DCPVideo> _queue;
	/** condition to manage thread wakeups when we have nothing to do */
	boost::condition _empty_condition;
	/** condition to manage thread wakeups when we have too much to do */
	boost::condition _full_condition;

	Waker _waker;

	EnumIndexedVector<std::shared_ptr<PlayerVideo>, Eyes> _last_player_video;

	boost::signals2::scoped_connection _server_found_connection;

#ifdef DCPOMATIC_GROK
	grk_plugin::DcpomaticContext* _dcpomatic_context = nullptr;
	grk_plugin::GrokContext *_context = nullptr;
	std::atomic<bool> _give_up;
#endif

	bool _ending = false;
};


#endif
