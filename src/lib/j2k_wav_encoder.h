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
#ifdef HAVE_SWRESAMPLE
extern "C" {
#include <libswresample/swresample.h>
}
#endif
#include <sndfile.h>
#include "encoder.h"

class ServerDescription;
class DCPVideoFrame;
class Image;
class Log;
class Subtitle;
class AudioBuffers;

/** @class J2KWAVEncoder
 *  @brief An encoder which writes JPEG2000 and WAV files.
 */
class J2KWAVEncoder : public Encoder
{
public:
	J2KWAVEncoder (boost::shared_ptr<const Film>, boost::shared_ptr<const Options>);
	~J2KWAVEncoder ();

	void process_begin (int64_t audio_channel_layout);
	void process_end ();

private:

	void do_process_video (boost::shared_ptr<const Image>, SourceFrame, boost::shared_ptr<Subtitle>);
	void do_process_audio (boost::shared_ptr<const AudioBuffers>);
	
	void write_audio (boost::shared_ptr<const AudioBuffers> audio);
	void encoder_thread (ServerDescription *);
	void close_sound_files ();
	void terminate_worker_threads ();

#if HAVE_SWRESAMPLE	
	SwrContext* _swr_context;
#endif	

	std::vector<SNDFILE*> _sound_files;
	int64_t _audio_frames_written;

	bool _process_end;
	std::list<boost::shared_ptr<DCPVideoFrame> > _queue;
	std::list<boost::thread *> _worker_threads;
	mutable boost::mutex _worker_mutex;
	boost::condition _worker_condition;
};
