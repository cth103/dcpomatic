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

/** @file  src/j2k_wav_encoder.h
 *  @brief An encoder which writes JPEG2000 and WAV files.
 */

#include <list>
#include <vector>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>
#include <sndfile.h>
#include "encoder.h"

class Server;
class DCPVideoFrame;
class Image;
class Log;

/** @class J2KWAVEncoder
 *  @brief An encoder which writes JPEG2000 and WAV files.
 */
class J2KWAVEncoder : public Encoder
{
public:
	J2KWAVEncoder (boost::shared_ptr<const FilmState>, boost::shared_ptr<const Options>, Log *);
	~J2KWAVEncoder ();

	void process_begin ();
	void process_video (boost::shared_ptr<Image>, int);
	void process_audio (uint8_t *, int);
	void process_end ();

private:	

	void encoder_thread (Server *);
	void close_sound_files ();
	void terminate_worker_threads ();

	std::vector<SNDFILE*> _sound_files;
	int _deinterleave_buffer_size;
	uint8_t* _deinterleave_buffer;

	bool _process_end;
	std::list<boost::shared_ptr<DCPVideoFrame> > _queue;
	std::list<boost::thread *> _worker_threads;
	mutable boost::mutex _worker_mutex;
	boost::condition _worker_condition;
};
