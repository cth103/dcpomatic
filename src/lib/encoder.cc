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
#include "film.h"
#include "log.h"
#include "exceptions.h"
#include "filter.h"
#include "config.h"
#include "dcp_video_frame.h"
#include "server.h"
#include "format.h"
#include "cross.h"
#include "writer.h"
#include "playlist.h"

#include "i18n.h"

using std::pair;
using std::string;
using std::stringstream;
using std::vector;
using std::list;
using std::cout;
using std::make_pair;
using namespace boost;

int const Encoder::_history_size = 25;

/** @param f Film that we are encoding */
Encoder::Encoder (shared_ptr<Film> f)
	: _film (f)
	, _video_frames_in (0)
	, _video_frames_out (0)
#ifdef HAVE_SWRESAMPLE	  
	, _swr_context (0)
#endif
	, _have_a_real_frame (false)
	, _terminate (false)
{
	
}

Encoder::~Encoder ()
{
	terminate_threads ();
	if (_writer) {
		_writer->finish ();
	}
}

void
Encoder::process_begin ()
{
	if (_film->has_audio() && _film->audio_frame_rate() != _film->target_audio_sample_rate()) {
#ifdef HAVE_SWRESAMPLE

		stringstream s;
		s << String::compose (N_("Will resample audio from %1 to %2"), _film->audio_frame_rate(), _film->target_audio_sample_rate());
		_film->log()->log (s.str ());

		/* We will be using planar float data when we call the resampler */
		_swr_context = swr_alloc_set_opts (
			0,
			_film->audio_channel_layout(),
			AV_SAMPLE_FMT_FLTP,
			_film->target_audio_sample_rate(),
			_film->audio_channel_layout(),
			AV_SAMPLE_FMT_FLTP,
			_film->audio_frame_rate(),
			0, 0
			);
		
		swr_init (_swr_context);
#else
		throw EncodeError (_("Cannot resample audio as libswresample is not present"));
#endif
	} else {
#ifdef HAVE_SWRESAMPLE
		_swr_context = 0;
#endif		
	}

	for (int i = 0; i < Config::instance()->num_local_encoding_threads (); ++i) {
		_threads.push_back (new boost::thread (boost::bind (&Encoder::encoder_thread, this, (ServerDescription *) 0)));
	}

	vector<ServerDescription*> servers = Config::instance()->servers ();

	for (vector<ServerDescription*>::iterator i = servers.begin(); i != servers.end(); ++i) {
		for (int j = 0; j < (*i)->threads (); ++j) {
			_threads.push_back (new boost::thread (boost::bind (&Encoder::encoder_thread, this, *i)));
		}
	}

	_writer.reset (new Writer (_film));
}


void
Encoder::process_end ()
{
#if HAVE_SWRESAMPLE	
	if (_film->has_audio() && _film->audio_channels() && _swr_context) {

		shared_ptr<AudioBuffers> out (new AudioBuffers (_film->audio_channels(), 256));
			
		while (1) {
			int const frames = swr_convert (_swr_context, (uint8_t **) out->data(), 256, 0, 0);

			if (frames < 0) {
				throw EncodeError (_("could not run sample-rate converter"));
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

	boost::mutex::scoped_lock lock (_mutex);

	_film->log()->log (String::compose (N_("Clearing queue of %1"), _queue.size ()));

	/* Keep waking workers until the queue is empty */
	while (!_queue.empty ()) {
		_film->log()->log (String::compose (N_("Waking with %1"), _queue.size ()), Log::VERBOSE);
		_condition.notify_all ();
		_condition.wait (lock);
	}

	lock.unlock ();
	
	terminate_threads ();

	_film->log()->log (String::compose (N_("Mopping up %1"), _queue.size()));

	/* The following sequence of events can occur in the above code:
	     1. a remote worker takes the last image off the queue
	     2. the loop above terminates
	     3. the remote worker fails to encode the image and puts it back on the queue
	     4. the remote worker is then terminated by terminate_threads

	     So just mop up anything left in the queue here.
	*/

	for (list<shared_ptr<DCPVideoFrame> >::iterator i = _queue.begin(); i != _queue.end(); ++i) {
		_film->log()->log (String::compose (N_("Encode left-over frame %1"), (*i)->frame ()));
		try {
			_writer->write ((*i)->encode_locally(), (*i)->frame ());
			frame_done ();
		} catch (std::exception& e) {
			_film->log()->log (String::compose (N_("Local encode failed (%1)"), e.what ()));
		}
	}

	_writer->finish ();
	_writer.reset ();
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
	
	struct timeval tv;
	gettimeofday (&tv, 0);
	_time_history.push_front (tv);
	if (int (_time_history.size()) > _history_size) {
		_time_history.pop_back ();
	}
}

void
Encoder::process_video (shared_ptr<Image> image, bool same, boost::shared_ptr<Subtitle> sub)
{
	FrameRateConversion frc (_film->video_frame_rate(), _film->dcp_frame_rate());
	
	if (frc.skip && (_video_frames_in % 2)) {
		++_video_frames_in;
		return;
	}

	boost::mutex::scoped_lock lock (_mutex);

	/* Wait until the queue has gone down a bit */
	while (_queue.size() >= _threads.size() * 2 && !_terminate) {
		TIMING ("decoder sleeps with queue of %1", _queue.size());
		_condition.wait (lock);
		TIMING ("decoder wakes with queue of %1", _queue.size());
	}

	if (_terminate) {
		return;
	}

	if (_writer->thrown ()) {
		_writer->rethrow ();
	}

	if (_writer->can_fake_write (_video_frames_out)) {
		_writer->fake_write (_video_frames_out);
		_have_a_real_frame = false;
		frame_done ();
	} else if (same && _have_a_real_frame) {
		/* Use the last frame that we encoded. */
		_writer->repeat (_video_frames_out);
		frame_done ();
	} else {
		/* Queue this new frame for encoding */
		pair<string, string> const s = Filter::ffmpeg_strings (_film->filters());
		TIMING ("adding to queue of %1", _queue.size ());
		_queue.push_back (boost::shared_ptr<DCPVideoFrame> (
					  new DCPVideoFrame (
						  image, sub, _film->format()->dcp_size(), _film->format()->dcp_padding (_film),
						  _film->subtitle_offset(), _film->subtitle_scale(),
						  _film->scaler(), _video_frames_out, _film->dcp_frame_rate(), s.second,
						  _film->colour_lut(), _film->j2k_bandwidth(),
						  _film->log()
						  )
					  ));
		
		_condition.notify_all ();
		_have_a_real_frame = true;
	}

	++_video_frames_in;
	++_video_frames_out;

	if (frc.repeat) {
		_writer->repeat (_video_frames_out);
		++_video_frames_out;
		frame_done ();
	}
}

void
Encoder::process_audio (shared_ptr<AudioBuffers> data)
{
#if HAVE_SWRESAMPLE
	/* Maybe sample-rate convert */
	if (_swr_context) {

		/* Compute the resampled frames count and add 32 for luck */
		int const max_resampled_frames = ceil ((int64_t) data->frames() * _film->target_audio_sample_rate() / _film->audio_frame_rate()) + 32;

		shared_ptr<AudioBuffers> resampled (new AudioBuffers (_film->audio_channels(), max_resampled_frames));

		/* Resample audio */
		int const resampled_frames = swr_convert (
			_swr_context, (uint8_t **) resampled->data(), max_resampled_frames, (uint8_t const **) data->data(), data->frames()
			);
		
		if (resampled_frames < 0) {
			throw EncodeError (_("could not run sample-rate converter"));
		}

		resampled->set_frames (resampled_frames);
		
		/* And point our variables at the resampled audio */
		data = resampled;
	}
#endif

	write_audio (data);
}

void
Encoder::terminate_threads ()
{
	boost::mutex::scoped_lock lock (_mutex);
	_terminate = true;
	_condition.notify_all ();
	lock.unlock ();

	for (list<boost::thread *>::iterator i = _threads.begin(); i != _threads.end(); ++i) {
		(*i)->join ();
		delete *i;
	}
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
		boost::mutex::scoped_lock lock (_mutex);
		while (_queue.empty () && !_terminate) {
			_condition.wait (lock);
		}

		if (_terminate) {
			return;
		}

		TIMING ("encoder thread %1 wakes with queue of %2", boost::this_thread::get_id(), _queue.size());
		boost::shared_ptr<DCPVideoFrame> vf = _queue.front ();
		_film->log()->log (String::compose (N_("Encoder thread %1 pops frame %2 from queue"), boost::this_thread::get_id(), vf->frame()), Log::VERBOSE);
		_queue.pop_front ();
		
		lock.unlock ();

		shared_ptr<EncodedData> encoded;

		if (server) {
			try {
				encoded = vf->encode_remotely (server);

				if (remote_backoff > 0) {
					_film->log()->log (String::compose (N_("%1 was lost, but now she is found; removing backoff"), server->host_name ()));
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
						N_("Remote encode of %1 on %2 failed (%3); thread sleeping for %4s"),
						vf->frame(), server->host_name(), e.what(), remote_backoff)
					);
			}
				
		} else {
			try {
				TIMING ("encoder thread %1 begins local encode of %2", boost::this_thread::get_id(), vf->frame());
				encoded = vf->encode_locally ();
				TIMING ("encoder thread %1 finishes local encode of %2", boost::this_thread::get_id(), vf->frame());
			} catch (std::exception& e) {
				_film->log()->log (String::compose (N_("Local encode failed (%1)"), e.what ()));
			}
		}

		if (encoded) {
			_writer->write (encoded, vf->frame ());
			frame_done ();
		} else {
			lock.lock ();
			_film->log()->log (
				String::compose (N_("Encoder thread %1 pushes frame %2 back onto queue after failure"), boost::this_thread::get_id(), vf->frame())
				);
			_queue.push_front (vf);
			lock.unlock ();
		}

		if (remote_backoff > 0) {
			dvdomatic_sleep (remote_backoff);
		}

		lock.lock ();
		_condition.notify_all ();
	}
}

void
Encoder::write_audio (shared_ptr<const AudioBuffers> data)
{
	AudioMapping m (_film->audio_channels ());
	if (m.dcp_channels() != _film->audio_channels()) {

		/* Remap (currently just for mono -> 5.1) */

		shared_ptr<AudioBuffers> b (new AudioBuffers (m.dcp_channels(), data->frames ()));
		for (int i = 0; i < m.dcp_channels(); ++i) {
			optional<int> s = m.dcp_to_source (static_cast<libdcp::Channel> (i));
			if (!s) {
				b->make_silent (i);
			} else {
				memcpy (b->data()[i], data->data()[s.get()], data->frames() * sizeof(float));
			}
		}

		data = b;
	}

	_writer->write (data);
}
