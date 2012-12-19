/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/** @file  src/j2k_video_encoder.h
 *  @brief An encoder which writes JPEG2000 files, where they are video (ie not still).
 */

#include <list>
#include <vector>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>
#include "encoder.h"

class ServerDescription;
class DCPVideoFrame;
class Image;
class Log;
class Subtitle;

/** @class J2KVideoEncoder
 *  @brief An encoder which writes JPEG2000 files, where they are video (ie not still).
 */
class J2KVideoEncoder : public Encoder
{
public:
	J2KVideoEncoder (boost::shared_ptr<const Film>, boost::shared_ptr<const EncodeOptions>);
	~J2KVideoEncoder ();

	void process_begin ();
	void process_end ();

private:

	void do_process_video (boost::shared_ptr<Image>, boost::shared_ptr<Subtitle>);
	
	void encoder_thread (ServerDescription *);
	void terminate_worker_threads ();

	bool _process_end;
	std::list<boost::shared_ptr<DCPVideoFrame> > _queue;
	std::list<boost::thread *> _worker_threads;
	mutable boost::mutex _worker_mutex;
	boost::condition _worker_condition;
};
