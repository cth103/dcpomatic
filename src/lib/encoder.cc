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
#include <libdcp/picture_asset.h>
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
#include "format.h"
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
unsigned int const Encoder::_maximum_frames_in_memory = 8;

/** @param f Film that we are encoding.
 *  @param o Options.
 */
Encoder::Encoder (shared_ptr<const Film> f)
	: _film (f)
	, _just_skipped (false)
	, _video_frames_in (0)
	, _audio_frames_in (0)
	, _video_frames_out (0)
	, _audio_frames_out (0)
#ifdef HAVE_SWRESAMPLE	  
	, _swr_context (0)
#endif
	, _have_a_real_frame (false)
	, _terminate_encoder (false)
	, _writer_thread (0)
	, _finish_writer (false)
	, _last_written_frame (-1)
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
			SNDFILE* f = sf_open (_film->multichannel_audio_out_path (i, true).c_str (), SFM_WRITE, &sf_info);
			if (f == 0) {
				throw CreateFileError (_film->multichannel_audio_out_path (i, true));
			}
			_sound_files.push_back (f);
		}
	}
}

Encoder::~Encoder ()
{
	close_sound_files ();
	terminate_worker_threads ();
	finish_writer_thread ();
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

	/* XXX! */
	_picture_asset.reset (
		new libdcp::MonoPictureAsset (
			_film->dir (_film->dcp_name()),
			String::compose ("video_%1.mxf", 0),
			DCPFrameRate (_film->frames_per_second()).frames_per_second,
			_film->format()->dcp_size()
			)
		);
	
	_picture_asset_writer = _picture_asset->start_write ();
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
			if (boost::filesystem::exists (_film->multichannel_audio_out_path (i, false))) {
				boost::filesystem::remove (_film->multichannel_audio_out_path (i, false));
			}
			boost::filesystem::rename (_film->multichannel_audio_out_path (i, true), _film->multichannel_audio_out_path (i, false));
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
			{
				boost::mutex::scoped_lock lock2 (_writer_mutex);
				_write_queue.push_back (make_pair (e, (*i)->frame ()));
				_writer_condition.notify_all ();
			}
			frame_done ();
		} catch (std::exception& e) {
			_film->log()->log (String::compose ("Local encode failed (%1)", e.what ()));
		}
	}

	finish_writer_thread ();
	_picture_asset_writer->finalize ();
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

/** @return Number of video frames that have been sent out */
int
Encoder::video_frames_out () const
{
	boost::mutex::scoped_lock (_history_mutex);
	return _video_frames_out;
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
	DCPFrameRate dfr (_film->frames_per_second ());
	
	if (dfr.skip && (_video_frames_in % 2)) {
		++_video_frames_in;
		return;
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
	if (boost::filesystem::exists (_film->frame_out_path (_video_frames_out, false))) {
		frame_skipped ();
		return;
	}

	if (same && _have_a_real_frame) {
		/* Use the last frame that we encoded.  We do this by putting a null encoded
		   frame straight onto the writer's queue.  It will know to duplicate the previous frame
		   in this case.
		*/
		boost::mutex::scoped_lock lock2 (_writer_mutex);
		_write_queue.push_back (make_pair (shared_ptr<EncodedData> (), _video_frames_out));
	} else {
		/* Queue this new frame for encoding */
		pair<string, string> const s = Filter::ffmpeg_strings (_film->filters());
		TIMING ("adding to queue of %1", _encode_queue.size ());
		_encode_queue.push_back (boost::shared_ptr<DCPVideoFrame> (
					  new DCPVideoFrame (
						  image, sub, _film->format()->dcp_size(), _film->format()->dcp_padding (_film),
						  _film->subtitle_offset(), _film->subtitle_scale(),
						  _film->scaler(), _video_frames_out, _film->frames_per_second(), s.second,
						  _film->colour_lut(), _film->j2k_bandwidth(),
						  _film->log()
						  )
					  ));
		
		_worker_condition.notify_all ();
		_have_a_real_frame = true;
	}

	++_video_frames_in;
	++_video_frames_out;

	if (dfr.repeat) {
		boost::mutex::scoped_lock lock2 (_writer_mutex);
		_write_queue.push_back (make_pair (shared_ptr<EncodedData> (), _video_frames_out));
		++_video_frames_out;
	}
}

void
Encoder::process_audio (shared_ptr<AudioBuffers> data)
{
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
	
	_audio_frames_in += data->frames ();
}

void
Encoder::write_audio (shared_ptr<const AudioBuffers> audio)
{
	for (int i = 0; i < audio->channels(); ++i) {
		sf_write_float (_sound_files[i], audio->data(i), audio->frames());
	}

	_audio_frames_out += audio->frames ();
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
Encoder::finish_writer_thread ()
{
	if (!_writer_thread) {
		return;
	}
	
	boost::mutex::scoped_lock lock (_writer_mutex);
	_finish_writer = true;
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

struct WriteQueueSorter
{
	bool operator() (pair<shared_ptr<EncodedData>, int> const & a, pair<shared_ptr<EncodedData>, int> const & b) {
		return a.second < b.second;
	}
};

void
Encoder::writer_thread ()
{
	while (1)
	{
		boost::mutex::scoped_lock lock (_writer_mutex);

		while (1) {
			if (_finish_writer ||
			    _write_queue.size() > _maximum_frames_in_memory ||
			    (!_write_queue.empty() && _write_queue.front().second == (_last_written_frame + 1))) {
				    
				    break;
			    }

			    TIMING ("writer sleeps with a queue of %1; %2 pending", _write_queue.size(), _pending.size());
			    _writer_condition.wait (lock);
			    TIMING ("writer wakes with a queue of %1", _write_queue.size());

			    _write_queue.sort (WriteQueueSorter ());
		}

		if (_finish_writer && _write_queue.empty() && _pending.empty()) {
			return;
		}

		/* Write any frames that we can write; i.e. those that are in sequence */
		while (!_write_queue.empty() && _write_queue.front().second == (_last_written_frame + 1)) {
			pair<boost::shared_ptr<EncodedData>, int> encoded = _write_queue.front ();
			_write_queue.pop_front ();

			lock.unlock ();
			/* XXX: write to mxf */
			_film->log()->log (String::compose ("Writer writes %1 to MXF", encoded.second));
			if (encoded.first) {
				_picture_asset_writer->write (encoded.first->data(), encoded.first->size());
				_last_written = encoded.first;
			} else {
				_picture_asset_writer->write (_last_written->data(), _last_written->size());
			}
			lock.lock ();

			++_last_written_frame;
		}

		while (_write_queue.size() > _maximum_frames_in_memory) {
			/* Too many frames in memory which can't yet be written to the stream.
			   Put some to disk.
			*/

			pair<boost::shared_ptr<EncodedData>, int> encoded = _write_queue.front ();
			_write_queue.pop_back ();
			if (!encoded.first) {
				/* This is a `repeat-last' frame, so no need to write it to disk */
				continue;
			}

			lock.unlock ();
			_film->log()->log (String::compose ("Writer full; pushes %1 to disk", encoded.second));
			encoded.first->write (_film, encoded.second);
			lock.lock ();

			_pending.push_back (encoded.second);
		}

		while (_write_queue.size() < _maximum_frames_in_memory && !_pending.empty()) {
			/* We have some space in memory.  Fetch some frames back off disk. */

			_pending.sort ();
			int const fetch = _pending.front ();

			lock.unlock ();
			_film->log()->log (String::compose ("Writer pulls %1 back from disk", fetch));
			shared_ptr<EncodedData> encoded;
			if (boost::filesystem::exists (_film->frame_out_path (fetch, false))) {
				/* It's an actual frame (not a repeat-last); load it in */
				encoded.reset (new EncodedData (_film->frame_out_path (fetch, false)));
			}
			lock.lock ();

			_write_queue.push_back (make_pair (encoded, fetch));
			_pending.remove (fetch);
		}
	}
}
