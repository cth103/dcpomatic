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

/** @file src/encoder.h
 *  @brief Parent class for classes which can encode video and audio frames.
 */

#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include "encoder.h"
#include "util.h"
#include "options.h"
#include "film.h"
#include "log.h"
#include "exceptions.h"
#include "filter.h"
#include "config.h"
#include "dcp_video_frame.h"
#include "server.h"
#include "cross.h"

using std::pair;
using std::string;
using std::stringstream;
using std::vector;
using std::list;
using std::cout;
using std::make_pair;
using namespace boost;

int const Encoder::_history_size = 25;

/** @param f Film that we are encoding.
 *  @param o Options.
 */
Encoder::Encoder (shared_ptr<const Film> f, shared_ptr<const EncodeOptions> o)
	: _film (f)
	, _opt (o)
	, _just_skipped (false)
	, _video_frame (0)
	, _audio_frame (0)
#ifdef HAVE_SWRESAMPLE	  
	, _swr_context (0)
#endif	  
	, _audio_frames_written (0)
	, _terminate_encoder (false)
	, _writer_thread (0)
	, _terminate_writer (false)
{
	if (_film->audio_stream()) {
		/* Create sound output files with .tmp suffixes; we will rename
		   them if and when we complete.
		*/
		for (int i = 0; i < dcp_audio_channels (_film->audio_channels()); ++i) {
			SF_INFO sf_info;
			sf_info.samplerate = dcp_audio_sample_rate (_film->audio_stream()->sample_rate());
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

Encoder::~Encoder ()
{
	close_sound_files ();
	terminate_worker_threads ();
	terminate_writer_thread ();
}

void
Encoder::process_begin ()
{
	if (_film->audio_stream() && _film->audio_stream()->sample_rate() != _film->target_audio_sample_rate()) {
#ifdef HAVE_SWRESAMPLE

		stringstream s;
		s << "Will resample audio from " << _film->audio_stream()->sample_rate() << " to " << _film->target_audio_sample_rate();
		_film->log()->log (s.str ());

		/* We will be using planar float data when we call the resampler */
		_swr_context = swr_alloc_set_opts (
			0,
			_film->audio_stream()->channel_layout(),
			AV_SAMPLE_FMT_FLTP,
			_film->target_audio_sample_rate(),
			_film->audio_stream()->channel_layout(),
			AV_SAMPLE_FMT_FLTP,
			_film->audio_stream()->sample_rate(),
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
		_worker_threads.push_back (new boost::thread (boost::bind (&Encoder::encoder_thread, this, (ServerDescription *) 0)));
	}

	vector<ServerDescription*> servers = Config::instance()->servers ();

	for (vector<ServerDescription*>::iterator i = servers.begin(); i != servers.end(); ++i) {
		for (int j = 0; j < (*i)->threads (); ++j) {
			_worker_threads.push_back (new boost::thread (boost::bind (&Encoder::encoder_thread, this, *i)));
		}
	}

	_writer_thread = new boost::thread (boost::bind (&Encoder::writer_thread, this));
}


void
Encoder::process_end ()
{
#if HAVE_SWRESAMPLE	
	if (_film->audio_stream() && _film->audio_stream()->channels() && _swr_context) {

		shared_ptr<AudioBuffers> out (new AudioBuffers (_film->audio_stream()->channels(), 256));
			
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
		close_sound_files ();
		
		/* Rename .wav.tmp files to .wav */
		for (int i = 0; i < dcp_audio_channels (_film->audio_channels()); ++i) {
			if (boost::filesystem::exists (_opt->multichannel_audio_out_path (i, false))) {
				boost::filesystem::remove (_opt->multichannel_audio_out_path (i, false));
			}
			boost::filesystem::rename (_opt->multichannel_audio_out_path (i, true), _opt->multichannel_audio_out_path (i, false));
		}
	}

	boost::mutex::scoped_lock lock (_worker_mutex);

	_film->log()->log ("Clearing queue of " + lexical_cast<string> (_encode_queue.size ()));

	/* Keep waking workers until the queue is empty */
	while (!_encode_queue.empty ()) {
		_film->log()->log ("Waking with " + lexical_cast<string> (_encode_queue.size ()), Log::VERBOSE);
		_worker_condition.notify_all ();
		_worker_condition.wait (lock);
	}

	lock.unlock ();
	
	terminate_worker_threads ();
	terminate_writer_thread ();

	_film->log()->log ("Mopping up " + lexical_cast<string> (_encode_queue.size()));

	/* The following sequence of events can occur in the above code:
	     1. a remote worker takes the last image off the queue
	     2. the loop above terminates
	     3. the remote worker fails to encode the image and puts it back on the queue
	     4. the remote worker is then terminated by terminate_worker_threads

	     So just mop up anything left in the queue here.
	*/

	for (list<shared_ptr<DCPVideoFrame> >::iterator i = _encode_queue.begin(); i != _encode_queue.end(); ++i) {
		_film->log()->log (String::compose ("Encode left-over frame %1", (*i)->frame ()));
		try {
			shared_ptr<EncodedData> e = (*i)->encode_locally ();
			e->write (_opt, (*i)->frame ());
			frame_done ();
		} catch (std::exception& e) {
			_film->log()->log (String::compose ("Local encode failed (%1)", e.what ()));
		}
	}

	/* Mop up any unwritten things in the writer's queue */
	for (list<pair<shared_ptr<EncodedData>, int> >::iterator i = _write_queue.begin(); i != _write_queue.end(); ++i) {
		i->first->write (_opt, i->second);
	}

	/* Now do links (or copies on windows) to duplicate frames */
	for (list<pair<int, int> >::iterator i = _links_required.begin(); i != _links_required.end(); ++i) {
		link (_opt->frame_out_path (i->first, false), _opt->frame_out_path (i->second, false));
		link (_opt->hash_out_path (i->first, false), _opt->hash_out_path (i->second, false));
	}
}	

/** @return an estimate of the current number of frames we are encoding per second,
 *  or 0 if not known.
 */
float
Encoder::current_frames_per_second () const
{
	boost::mutex::scoped_lock lock (_history_mutex);
	if (int (_time_history.size()) < _history_size) {
		return 0;
	}

	struct timeval now;
	gettimeofday (&now, 0);

	return _history_size / (seconds (now) - seconds (_time_history.back ()));
}

/** @return true if the last frame to be processed was skipped as it already existed */
bool
Encoder::skipping () const
{
	boost::mutex::scoped_lock (_history_mutex);
	return _just_skipped;
}

/** @return Number of video frames that have been received */
SourceFrame
Encoder::video_frame () const
{
	boost::mutex::scoped_lock (_history_mutex);
	return _video_frame;
}

/** Should be called when a frame has been encoded successfully.
 *  @param n Source frame index.
 */
void
Encoder::frame_done ()
{
	boost::mutex::scoped_lock lock (_history_mutex);
	_just_skipped = false;
	
	struct timeval tv;
	gettimeofday (&tv, 0);
	_time_history.push_front (tv);
	if (int (_time_history.size()) > _history_size) {
		_time_history.pop_back ();
	}
}

/** Called by a subclass when it has just skipped the processing
    of a frame because it has already been done.
*/
void
Encoder::frame_skipped ()
{
	boost::mutex::scoped_lock lock (_history_mutex);
	_just_skipped = true;
}

void
Encoder::process_video (shared_ptr<Image> image, bool same, boost::shared_ptr<Subtitle> sub)
{
	if (_opt->video_skip != 0 && (_video_frame % _opt->video_skip) != 0) {
		++_video_frame;
		return;
	}

	if (_opt->video_range) {
		pair<SourceFrame, SourceFrame> const r = _opt->video_range.get();
		if (_video_frame < r.first || _video_frame >= r.second) {
			++_video_frame;
			return;
		}
	}

	boost::mutex::scoped_lock lock (_worker_mutex);

	/* Wait until the queue has gone down a bit */
	while (_encode_queue.size() >= _worker_threads.size() * 2 && !_terminate_encoder) {
		TIMING ("decoder sleeps with queue of %1", _encode_queue.size());
		_worker_condition.wait (lock);
		TIMING ("decoder wakes with queue of %1", _encode_queue.size());
	}

	if (_terminate_encoder) {
		return;
	}

	/* Only do the processing if we don't already have a file for this frame */
	if (boost::filesystem::exists (_opt->frame_out_path (_video_frame, false))) {
		frame_skipped ();
		return;
	}

	if (same && _last_real_frame) {
		/* Use the last frame that we encoded.  We need to postpone doing the actual link,
		   as on windows the link is really a copy and the reference frame might not have
		   finished encoding yet.
		*/
		_links_required.push_back (make_pair (_last_real_frame.get(), _video_frame));
	} else {
		/* Queue this new frame for encoding */
		pair<string, string> const s = Filter::ffmpeg_strings (_film->filters());
		TIMING ("adding to queue of %1", _encode_queue.size ());
		_encode_queue.push_back (boost::shared_ptr<DCPVideoFrame> (
					  new DCPVideoFrame (
						  image, sub, _opt->out_size, _opt->padding, _film->subtitle_offset(), _film->subtitle_scale(),
						  _film->scaler(), _video_frame, _film->frames_per_second(), s.second,
						  Config::instance()->colour_lut_index (), Config::instance()->j2k_bandwidth (),
						  _film->log()
						  )
					  ));
		
		_worker_condition.notify_all ();
		_last_real_frame = _video_frame;
	}

	++_video_frame;
}

void
Encoder::process_audio (shared_ptr<AudioBuffers> data)
{
	if (_opt->audio_range) {
		shared_ptr<AudioBuffers> trimmed (new AudioBuffers (*data.get ()));
		
		/* Range that we are encoding */
		pair<int64_t, int64_t> required_range = _opt->audio_range.get();
		/* Range of this block of data */
		pair<int64_t, int64_t> this_range (_audio_frame, _audio_frame + trimmed->frames());

		if (this_range.second < required_range.first || required_range.second < this_range.first) {
			/* No part of this audio is within the required range */
			return;
		} else if (required_range.first >= this_range.first && required_range.first < this_range.second) {
			/* Trim start */
			int64_t const shift = required_range.first - this_range.first;
			trimmed->move (shift, 0, trimmed->frames() - shift);
			trimmed->set_frames (trimmed->frames() - shift);
		} else if (required_range.second >= this_range.first && required_range.second < this_range.second) {
			/* Trim end */
			trimmed->set_frames (required_range.second - this_range.first);
		}

		data = trimmed;
	}

#if HAVE_SWRESAMPLE
	/* Maybe sample-rate convert */
	if (_swr_context) {

		/* Compute the resampled frames count and add 32 for luck */
		int const max_resampled_frames = ceil ((int64_t) data->frames() * _film->target_audio_sample_rate() / _film->audio_stream()->sample_rate()) + 32;

		shared_ptr<AudioBuffers> resampled (new AudioBuffers (_film->audio_stream()->channels(), max_resampled_frames));

		/* Resample audio */
		int const resampled_frames = swr_convert (
			_swr_context, (uint8_t **) resampled->data(), max_resampled_frames, (uint8_t const **) data->data(), data->frames()
			);
		
		if (resampled_frames < 0) {
			throw EncodeError ("could not run sample-rate converter");
		}

		resampled->set_frames (resampled_frames);
		
		/* And point our variables at the resampled audio */
		data = resampled;
	}
#endif

	if (_film->audio_channels() == 1) {
		/* We need to switch things around so that the mono channel is on
		   the centre channel of a 5.1 set (with other channels silent).
		*/

		shared_ptr<AudioBuffers> b (new AudioBuffers (6, data->frames ()));
		b->make_silent (libdcp::LEFT);
		b->make_silent (libdcp::RIGHT);
		memcpy (b->data()[libdcp::CENTRE], data->data()[0], data->frames() * sizeof(float));
		b->make_silent (libdcp::LFE);
		b->make_silent (libdcp::LS);
		b->make_silent (libdcp::RS);

		data = b;
	}

	write_audio (data);
	
	_audio_frame += data->frames ();
}

void
Encoder::write_audio (shared_ptr<const AudioBuffers> audio)
{
	for (int i = 0; i < audio->channels(); ++i) {
		sf_write_float (_sound_files[i], audio->data(i), audio->frames());
	}

	_audio_frames_written += audio->frames ();
}

void
Encoder::close_sound_files ()
{
	for (vector<SNDFILE*>::iterator i = _sound_files.begin(); i != _sound_files.end(); ++i) {
		sf_close (*i);
	}

	_sound_files.clear ();
}	

void
Encoder::terminate_worker_threads ()
{
	boost::mutex::scoped_lock lock (_worker_mutex);
	_terminate_encoder = true;
	_worker_condition.notify_all ();
	lock.unlock ();

	for (list<boost::thread *>::iterator i = _worker_threads.begin(); i != _worker_threads.end(); ++i) {
		(*i)->join ();
		delete *i;
	}
}

void
Encoder::terminate_writer_thread ()
{
	if (!_writer_thread) {
		return;
	}
	
	boost::mutex::scoped_lock lock (_writer_mutex);
	_terminate_writer = true;
	_writer_condition.notify_all ();
	lock.unlock ();

	_writer_thread->join ();
	delete _writer_thread;
	_writer_thread = 0;
}

void
Encoder::encoder_thread (ServerDescription* server)
{
	/* Number of seconds that we currently wait between attempts
	   to connect to the server; not relevant for localhost
	   encodings.
	*/
	int remote_backoff = 0;
	
	while (1) {

		TIMING ("encoder thread %1 sleeps", boost::this_thread::get_id());
		boost::mutex::scoped_lock lock (_worker_mutex);
		while (_encode_queue.empty () && !_terminate_encoder) {
			_worker_condition.wait (lock);
		}

		if (_terminate_encoder) {
			return;
		}

		TIMING ("encoder thread %1 wakes with queue of %2", boost::this_thread::get_id(), _encode_queue.size());
		boost::shared_ptr<DCPVideoFrame> vf = _encode_queue.front ();
		_film->log()->log (String::compose ("Encoder thread %1 pops frame %2 from queue", boost::this_thread::get_id(), vf->frame()), Log::VERBOSE);
		_encode_queue.pop_front ();
		
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
			boost::mutex::scoped_lock lock2 (_writer_mutex);
			_write_queue.push_back (make_pair (encoded, vf->frame ()));
			_writer_condition.notify_all ();
		} else {
			lock.lock ();
			_film->log()->log (
				String::compose ("Encoder thread %1 pushes frame %2 back onto queue after failure", boost::this_thread::get_id(), vf->frame())
				);
			_encode_queue.push_front (vf);
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
Encoder::link (string a, string b) const
{
#ifdef DVDOMATIC_POSIX			
	int const r = symlink (a.c_str(), b.c_str());
	if (r) {
		throw EncodeError (String::compose ("could not create symlink from %1 to %2", a, b));
	}
#endif
	
#ifdef DVDOMATIC_WINDOWS
	boost::filesystem::copy_file (a, b);
#endif			
}

void
Encoder::writer_thread ()
{
	while (1)
	{
		TIMING ("writer sleeps");
		boost::mutex::scoped_lock lock (_writer_mutex);
		while (_write_queue.empty() && !_terminate_writer) {
			_writer_condition.wait (lock);
		}
		TIMING ("writer wakes with a queue of %1", _write_queue.size());

		if (_terminate_writer) {
			return;
		}

		pair<boost::shared_ptr<EncodedData>, int> encoded = _write_queue.front ();
		_write_queue.pop_front ();

		lock.unlock ();
		encoded.first->write (_opt, encoded.second);
		lock.lock ();
	}
}
