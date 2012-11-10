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
#include "options.h"
#include "exceptions.h"
#include "dcp_video_frame.h"
#include "server.h"
#include "filter.h"
#include "log.h"
#include "cross.h"
#include "film.h"

using std::string;
using std::stringstream;
using std::list;
using std::vector;
using std::pair;
using boost::shared_ptr;
using boost::thread;
using boost::lexical_cast;

J2KWAVEncoder::J2KWAVEncoder (shared_ptr<const Film> f, shared_ptr<const Options> o)
	: Encoder (f, o)
#ifdef HAVE_SWRESAMPLE	  
	, _swr_context (0)
#endif	  
	, _audio_frames_written (0)
	, _process_end (false)
{
	if (_film->audio_stream()) {
		/* Create sound output files with .tmp suffixes; we will rename
		   them if and when we complete.
		*/
		for (int i = 0; i < _film->audio_channels(); ++i) {
			SF_INFO sf_info;
			sf_info.samplerate = dcp_audio_sample_rate (_film->audio_stream().get().sample_rate());
			/* We write mono files */
			sf_info.channels = 1;
			sf_info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_24;
			SNDFILE* f = sf_open (_opt->multichannel_audio_out_path (i, true).c_str (), SFM_WRITE, &sf_info);
			if (f == 0) {
				throw CreateFileError (_opt->multichannel_audio_out_path (i, true));
			}
			_sound_files.push_back (f);
		}
	}
}

J2KWAVEncoder::~J2KWAVEncoder ()
{
	terminate_worker_threads ();
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
J2KWAVEncoder::do_process_video (shared_ptr<const Image> yuv, SourceFrame frame, shared_ptr<Subtitle> sub)
{
	boost::mutex::scoped_lock lock (_worker_mutex);

	/* Wait until the queue has gone down a bit */
	while (_queue.size() >= _worker_threads.size() * 2 && !_process_end) {
		TIMING ("decoder sleeps with queue of %1", _queue.size());
		_worker_condition.wait (lock);
		TIMING ("decoder wakes with queue of %1", _queue.size());
	}

	if (_process_end) {
		return;
	}

	/* Only do the processing if we don't already have a file for this frame */
	if (!boost::filesystem::exists (_opt->frame_out_path (frame, false))) {
		pair<string, string> const s = Filter::ffmpeg_strings (_film->filters());
		TIMING ("adding to queue of %1", _queue.size ());
		_queue.push_back (boost::shared_ptr<DCPVideoFrame> (
					  new DCPVideoFrame (
						  yuv, sub, _opt->out_size, _opt->padding, _film->subtitle_offset(), _film->subtitle_scale(),
						  _film->scaler(), frame, _film->frames_per_second(), s.second,
						  Config::instance()->colour_lut_index (), Config::instance()->j2k_bandwidth (),
						  _film->log()
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

		TIMING ("encoder thread %1 sleeps", boost::this_thread::get_id());
		boost::mutex::scoped_lock lock (_worker_mutex);
		while (_queue.empty () && !_process_end) {
			_worker_condition.wait (lock);
		}

		if (_process_end) {
			return;
		}

		TIMING ("encoder thread %1 wakes with queue of %2", boost::this_thread::get_id(), _queue.size());
		boost::shared_ptr<DCPVideoFrame> vf = _queue.front ();
		_film->log()->log (String::compose ("Encoder thread %1 pops frame %2 from queue", boost::this_thread::get_id(), vf->frame()), Log::VERBOSE);
		_queue.pop_front ();
		
		lock.unlock ();

		shared_ptr<EncodedData> encoded;

		if (server) {
			try {
				encoded = vf->encode_remotely (server);

				if (remote_backoff > 0) {
					_film->log()->log (String::compose ("%1 was lost, but now she is found; removing backoff", server->host_name ()));
				}
				
				/* This job succeeded, so remove any backoff */
				remote_backoff = 0;
				
			} catch (std::exception& e) {
				if (remote_backoff < 60) {
					/* back off more */
					remote_backoff += 10;
				}
				_film->log()->log (
					String::compose (
						"Remote encode of %1 on %2 failed (%3); thread sleeping for %4s",
						vf->frame(), server->host_name(), e.what(), remote_backoff)
					);
			}
				
		} else {
			try {
				TIMING ("encoder thread %1 begins local encode of %2", boost::this_thread::get_id(), vf->frame());
				encoded = vf->encode_locally ();
				TIMING ("encoder thread %1 finishes local encode of %2", boost::this_thread::get_id(), vf->frame());
			} catch (std::exception& e) {
				_film->log()->log (String::compose ("Local encode failed (%1)", e.what ()));
			}
		}

		if (encoded) {
			encoded->write (_opt, vf->frame ());
			frame_done (vf->frame ());
		} else {
			lock.lock ();
			_film->log()->log (
				String::compose ("Encoder thread %1 pushes frame %2 back onto queue after failure", boost::this_thread::get_id(), vf->frame())
				);
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
	if (_film->audio_stream() && _film->audio_stream().get().sample_rate() != _film->target_audio_sample_rate()) {
#ifdef HAVE_SWRESAMPLE

		stringstream s;
		s << "Will resample audio from " << _film->audio_stream().get().sample_rate() << " to " << _film->target_audio_sample_rate();
		_film->log()->log (s.str ());

		/* We will be using planar float data when we call the resampler */
		_swr_context = swr_alloc_set_opts (
			0,
			_film->audio_stream().get().channel_layout(),
			AV_SAMPLE_FMT_FLTP,
			_film->target_audio_sample_rate(),
			_film->audio_stream().get().channel_layout(),
			AV_SAMPLE_FMT_FLTP,
			_film->audio_stream().get().sample_rate(),
			0, 0
			);
		
		swr_init (_swr_context);
#else
		throw EncodeError ("Cannot resample audio as libswresample is not present");
#endif
	} else {
#ifdef HAVE_SWRESAMPLE
		_swr_context = 0;
#endif		
	}
	
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

	_film->log()->log ("Clearing queue of " + lexical_cast<string> (_queue.size ()));

	/* Keep waking workers until the queue is empty */
	while (!_queue.empty ()) {
		_film->log()->log ("Waking with " + lexical_cast<string> (_queue.size ()), Log::VERBOSE);
		_worker_condition.notify_all ();
		_worker_condition.wait (lock);
	}

	lock.unlock ();
	
	terminate_worker_threads ();

	_film->log()->log ("Mopping up " + lexical_cast<string> (_queue.size()));

	/* The following sequence of events can occur in the above code:
	     1. a remote worker takes the last image off the queue
	     2. the loop above terminates
	     3. the remote worker fails to encode the image and puts it back on the queue
	     4. the remote worker is then terminated by terminate_worker_threads

	     So just mop up anything left in the queue here.
	*/

	for (list<shared_ptr<DCPVideoFrame> >::iterator i = _queue.begin(); i != _queue.end(); ++i) {
		_film->log()->log (String::compose ("Encode left-over frame %1", (*i)->frame ()));
		try {
			shared_ptr<EncodedData> e = (*i)->encode_locally ();
			e->write (_opt, (*i)->frame ());
			frame_done ((*i)->frame ());
		} catch (std::exception& e) {
			_film->log()->log (String::compose ("Local encode failed (%1)", e.what ()));
		}
	}

#if HAVE_SWRESAMPLE	
	if (_film->audio_stream() && _swr_context) {

		shared_ptr<AudioBuffers> out (new AudioBuffers (_film->audio_stream().get().channels(), 256));
			
		while (1) {
			int const frames = swr_convert (_swr_context, (uint8_t **) out->data(), 256, 0, 0);

			if (frames < 0) {
				throw EncodeError ("could not run sample-rate converter");
			}

			if (frames == 0) {
				break;
			}

			out->set_frames (frames);
			write_audio (out);
		}

		swr_free (&_swr_context);
	}
#endif

	if (_film->audio_stream()) {
		int const dcp_sr = dcp_audio_sample_rate (_film->audio_stream().get().sample_rate ());
		int64_t const extra_audio_frames = dcp_sr - (_audio_frames_written % dcp_sr);
		shared_ptr<AudioBuffers> silence (new AudioBuffers (_film->audio_stream().get().channels(), extra_audio_frames));
		silence->make_silent ();
		write_audio (silence);
		
		close_sound_files ();
		
		/* Rename .wav.tmp files to .wav */
		for (int i = 0; i < _film->audio_channels(); ++i) {
			if (boost::filesystem::exists (_opt->multichannel_audio_out_path (i, false))) {
				boost::filesystem::remove (_opt->multichannel_audio_out_path (i, false));
			}
			boost::filesystem::rename (_opt->multichannel_audio_out_path (i, true), _opt->multichannel_audio_out_path (i, false));
		}
	}
}

void
J2KWAVEncoder::do_process_audio (shared_ptr<const AudioBuffers> audio)
{
	shared_ptr<AudioBuffers> resampled;
	
#if HAVE_SWRESAMPLE
	/* Maybe sample-rate convert */
	if (_swr_context) {

		/* Compute the resampled frames count and add 32 for luck */
		int const max_resampled_frames = ceil (audio->frames() * _film->target_audio_sample_rate() / _film->audio_stream().get().sample_rate()) + 32;

		resampled.reset (new AudioBuffers (_film->audio_stream().get().channels(), max_resampled_frames));

		/* Resample audio */
		int const resampled_frames = swr_convert (
			_swr_context, (uint8_t **) resampled->data(), max_resampled_frames, (uint8_t const **) audio->data(), audio->frames()
			);
		
		if (resampled_frames < 0) {
			throw EncodeError ("could not run sample-rate converter");
		}

		resampled->set_frames (resampled_frames);
		
		/* And point our variables at the resampled audio */
		audio = resampled;
	}
#endif

	write_audio (audio);
}

void
J2KWAVEncoder::write_audio (shared_ptr<const AudioBuffers> audio)
{
	for (int i = 0; i < _film->audio_channels(); ++i) {
		sf_write_float (_sound_files[i], audio->data(i), audio->frames());
	}

	_audio_frames_written += audio->frames ();
}

