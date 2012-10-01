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

/** @file  src/j2k_wav_encoder.cc
 *  @brief An encoder which writes JPEG2000 and WAV files.
 */

#include <sstream>
#include <stdexcept>
#include <iomanip>
#include <iostream>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <sndfile.h>
#include <openjpeg.h>
#include "j2k_wav_encoder.h"
#include "config.h"
#include "film_state.h"
#include "options.h"
#include "exceptions.h"
#include "dcp_video_frame.h"
#include "server.h"
#include "filter.h"
#include "log.h"
#include "cross.h"

using namespace std;
using namespace boost;

J2KWAVEncoder::J2KWAVEncoder (shared_ptr<const FilmState> s, shared_ptr<const Options> o, Log* l)
	: Encoder (s, o, l)
	, _deinterleave_buffer_size (8192)
	, _deinterleave_buffer (0)
	, _process_end (false)
{
	/* Create sound output files with .tmp suffixes; we will rename
	   them if and when we complete.
	*/
	for (int i = 0; i < _fs->audio_channels; ++i) {
		SF_INFO sf_info;
		sf_info.samplerate = dcp_audio_sample_rate (_fs->audio_sample_rate);
		/* We write mono files */
		sf_info.channels = 1;
		sf_info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_24;
		SNDFILE* f = sf_open (_opt->multichannel_audio_out_path (i, true).c_str (), SFM_WRITE, &sf_info);
		if (f == 0) {
			throw CreateFileError (_opt->multichannel_audio_out_path (i, true));
		}
		_sound_files.push_back (f);
	}

	/* Create buffer for deinterleaving audio */
	_deinterleave_buffer = new uint8_t[_deinterleave_buffer_size];
}

J2KWAVEncoder::~J2KWAVEncoder ()
{
	terminate_worker_threads ();
	delete[] _deinterleave_buffer;
	close_sound_files ();
}

void
J2KWAVEncoder::terminate_worker_threads ()
{
	boost::mutex::scoped_lock lock (_worker_mutex);
	_process_end = true;
	_worker_condition.notify_all ();
	lock.unlock ();

	for (list<boost::thread *>::iterator i = _worker_threads.begin(); i != _worker_threads.end(); ++i) {
		(*i)->join ();
		delete *i;
	}
}

void
J2KWAVEncoder::close_sound_files ()
{
	for (vector<SNDFILE*>::iterator i = _sound_files.begin(); i != _sound_files.end(); ++i) {
		sf_close (*i);
	}

	_sound_files.clear ();
}	

void
J2KWAVEncoder::process_video (shared_ptr<Image> yuv, int frame)
{
	boost::mutex::scoped_lock lock (_worker_mutex);

	/* Wait until the queue has gone down a bit */
	while (_queue.size() >= _worker_threads.size() * 2 && !_process_end) {
		_log->microsecond_log ("Decoder sleeps", Log::TIMING);
		_worker_condition.wait (lock);
		_log->microsecond_log ("Decoder wakes", Log::TIMING);
	}

	if (_process_end) {
		return;
	}

	/* Only do the processing if we don't already have a file for this frame */
	if (!boost::filesystem::exists (_opt->frame_out_path (frame, false))) {
		pair<string, string> const s = Filter::ffmpeg_strings (_fs->filters);
		_log->microsecond_log ("Adding to queue of " + boost::lexical_cast<string> (_queue.size ()), Log::TIMING);
		_queue.push_back (boost::shared_ptr<DCPVideoFrame> (
					  new DCPVideoFrame (
						  yuv, _opt->out_size, _opt->padding, _fs->scaler, frame, _fs->frames_per_second, s.second,
						  Config::instance()->colour_lut_index (), Config::instance()->j2k_bandwidth (),
						  _log
						  )
					  ));
		
		_worker_condition.notify_all ();
	} else {
		frame_skipped ();
	}
}

void
J2KWAVEncoder::encoder_thread (ServerDescription* server)
{
	/* Number of seconds that we currently wait between attempts
	   to connect to the server; not relevant for localhost
	   encodings.
	*/
	int remote_backoff = 0;
	
	while (1) {
		boost::mutex::scoped_lock lock (_worker_mutex);
		while (_queue.empty () && !_process_end) {
			_log->microsecond_log ("Encoder thread sleeps", Log::TIMING);
			_worker_condition.wait (lock);
			_log->microsecond_log ("Encoder thread wakes", Log::TIMING);
		}

		if (_process_end) {
			return;
		}

		boost::shared_ptr<DCPVideoFrame> vf = _queue.front ();
		_log->microsecond_log ("Encoder thread wakes with queue of " + boost::lexical_cast<string> (_queue.size ()), Log::TIMING);
		_queue.pop_front ();
		
		lock.unlock ();

		shared_ptr<EncodedData> encoded;

		if (server) {
			try {
				encoded = vf->encode_remotely (server);

				if (remote_backoff > 0) {
					stringstream s;
					s << server->host_name() << " was lost, but now she is found; removing backoff";
					_log->log (s.str ());
				}
				
				/* This job succeeded, so remove any backoff */
				remote_backoff = 0;
				
			} catch (std::exception& e) {
				if (remote_backoff < 60) {
					/* back off more */
					remote_backoff += 10;
				}
				stringstream s;
				s << "Remote encode of " << vf->frame() << " on " << server->host_name() << " failed (" << e.what() << "); thread sleeping for " << remote_backoff << "s.";
				_log->log (s.str ());
			}
				
		} else {
			try {
				_log->microsecond_log ("Encoder thread begins local encode of " + lexical_cast<string> (vf->frame ()), Log::TIMING);
				encoded = vf->encode_locally ();
				_log->microsecond_log ("Encoder thread finishes local encode of " + lexical_cast<string> (vf->frame ()), Log::TIMING);
			} catch (std::exception& e) {
				stringstream s;
				s << "Local encode failed " << e.what() << ".";
				_log->log (s.str ());
			}
		}

		if (encoded) {
			encoded->write (_opt, vf->frame ());
			frame_done (vf->frame ());
		} else {
			lock.lock ();
			_queue.push_front (vf);
			lock.unlock ();
		}

		if (remote_backoff > 0) {
			dvdomatic_sleep (remote_backoff);
		}

		lock.lock ();
		_worker_condition.notify_all ();
	}
}

void
J2KWAVEncoder::process_begin ()
{
	for (int i = 0; i < Config::instance()->num_local_encoding_threads (); ++i) {
		_worker_threads.push_back (new boost::thread (boost::bind (&J2KWAVEncoder::encoder_thread, this, (ServerDescription *) 0)));
	}

	vector<ServerDescription*> servers = Config::instance()->servers ();

	for (vector<ServerDescription*>::iterator i = servers.begin(); i != servers.end(); ++i) {
		for (int j = 0; j < (*i)->threads (); ++j) {
			_worker_threads.push_back (new boost::thread (boost::bind (&J2KWAVEncoder::encoder_thread, this, *i)));
		}
	}
}

void
J2KWAVEncoder::process_end ()
{
	boost::mutex::scoped_lock lock (_worker_mutex);

	_log->log ("Clearing queue of " + lexical_cast<string> (_queue.size ()));

	/* Keep waking workers until the queue is empty */
	while (!_queue.empty ()) {
		_log->log ("Waking with " + lexical_cast<string> (_queue.size ()));
		_worker_condition.notify_all ();
		_worker_condition.wait (lock);
	}

	lock.unlock ();
	
	terminate_worker_threads ();

	_log->log ("Mopping up " + lexical_cast<string> (_queue.size()));

	/* The following sequence of events can occur in the above code:
	     1. a remote worker takes the last image off the queue
	     2. the loop above terminates
	     3. the remote worker fails to encode the image and puts it back on the queue
	     4. the remote worker is then terminated by terminate_worker_threads

	     So just mop up anything left in the queue here.
	*/

	for (list<shared_ptr<DCPVideoFrame> >::iterator i = _queue.begin(); i != _queue.end(); ++i) {
		stringstream s;
		s << "Encode left-over frame " << (*i)->frame();
		_log->log (s.str ());
		try {
			shared_ptr<EncodedData> e = (*i)->encode_locally ();
			e->write (_opt, (*i)->frame ());
			frame_done ((*i)->frame ());
		} catch (std::exception& e) {
			stringstream s;
			s << "Local encode failed " << e.what() << ".";
			_log->log (s.str ());
		}
	}
	
	close_sound_files ();

	/* Rename .wav.tmp files to .wav */
	for (int i = 0; i < _fs->audio_channels; ++i) {
		if (boost::filesystem::exists (_opt->multichannel_audio_out_path (i, false))) {
			boost::filesystem::remove (_opt->multichannel_audio_out_path (i, false));
		}
		boost::filesystem::rename (_opt->multichannel_audio_out_path (i, true), _opt->multichannel_audio_out_path (i, false));
	}
}

void
J2KWAVEncoder::process_audio (uint8_t* data, int data_size)
{
	/* Size of a sample in bytes */
	int const sample_size = 2;
	
	/* XXX: we are assuming that sample_size is right, the _deinterleave_buffer_size is a multiple
	   of the sample size and that data_size is a multiple of _fs->audio_channels * sample_size.
	*/
	
	/* XXX: this code is very tricksy and it must be possible to make it simpler ... */
	
	/* Number of bytes left to read this time */
	int remaining = data_size;
	/* Our position in the output buffers, in bytes */
	int position = 0;
	while (remaining > 0) {
		/* How many bytes of the deinterleaved data to do this time */
		int this_time = min (remaining / _fs->audio_channels, _deinterleave_buffer_size);
		for (int i = 0; i < _fs->audio_channels; ++i) {
			for (int j = 0; j < this_time; j += sample_size) {
				for (int k = 0; k < sample_size; ++k) {
					int const to = j + k;
					int const from = position + (i * sample_size) + (j * _fs->audio_channels) + k;
					_deinterleave_buffer[to] = data[from];
				}
			}
			
			switch (_fs->audio_sample_format) {
			case AV_SAMPLE_FMT_S16:
				sf_write_short (_sound_files[i], (const short *) _deinterleave_buffer, this_time / sample_size);
				break;
			default:
				throw DecodeError ("unknown audio sample format");
			}
		}
		
		position += this_time;
		remaining -= this_time * _fs->audio_channels;
	}
}
