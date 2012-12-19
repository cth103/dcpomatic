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

#include <boost/filesystem.hpp>
#include "encoder.h"
#include "util.h"
#include "options.h"
#include "film.h"
#include "log.h"
#include "exceptions.h"

using std::pair;
using std::stringstream;
using std::vector;
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
{
	if (_film->audio_stream()) {
		/* Create sound output files with .tmp suffixes; we will rename
		   them if and when we complete.
		*/
		for (int i = 0; i < _film->audio_channels(); ++i) {
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
}


void
Encoder::process_end ()
{
#if HAVE_SWRESAMPLE	
	if (_film->audio_stream() && _swr_context) {

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
		for (int i = 0; i < _film->audio_channels(); ++i) {
			if (boost::filesystem::exists (_opt->multichannel_audio_out_path (i, false))) {
				boost::filesystem::remove (_opt->multichannel_audio_out_path (i, false));
			}
			boost::filesystem::rename (_opt->multichannel_audio_out_path (i, true), _opt->multichannel_audio_out_path (i, false));
		}
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
Encoder::process_video (shared_ptr<Image> i, boost::shared_ptr<Subtitle> s)
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

	do_process_video (i, s);
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

	write_audio (data);
	
	_audio_frame += data->frames ();
}

void
Encoder::write_audio (shared_ptr<const AudioBuffers> audio)
{
	for (int i = 0; i < _film->audio_channels(); ++i) {
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

