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

#include <libdcp/picture_asset.h>
#include <libdcp/sound_asset.h>
#include <libdcp/reel.h>
#include "writer.h"
#include "compose.hpp"
#include "film.h"
#include "format.h"
#include "log.h"
#include "dcp_video_frame.h"

using std::make_pair;
using std::pair;
using boost::shared_ptr;

unsigned int const Writer::_maximum_frames_in_memory = 8;

Writer::Writer (shared_ptr<const Film> f)
	: _film (f)
	, _thread (0)
	, _finish (false)
	, _last_written_frame (-1)
{
	_picture_asset.reset (
		new libdcp::MonoPictureAsset (
			_film->dir (_film->dcp_name()),
			String::compose ("video_%1.mxf", 0),
			DCPFrameRate (_film->frames_per_second()).frames_per_second,
			_film->format()->dcp_size()
			)
		);
	
	_picture_asset_writer = _picture_asset->start_write ();

	if (_film->audio_channels() > 0) {
		_sound_asset.reset (
			new libdcp::SoundAsset (
				_film->dir (_film->dcp_name()),
				String::compose ("audio_%1.mxf", 0),
				DCPFrameRate (_film->frames_per_second()).frames_per_second,
				_film->audio_channels(),
				_film->audio_stream()->sample_rate()
				)
			);

		_sound_asset_writer = _sound_asset->start_write ();
	}
	
	_thread = new boost::thread (boost::bind (&Writer::thread, this));
}

void
Writer::write (shared_ptr<const EncodedData> encoded, int frame)
{
	boost::mutex::scoped_lock lock (_mutex);
	_queue.push_back (make_pair (encoded, frame));
	_condition.notify_all ();
}

/** This method is not thread safe */
void
Writer::write (shared_ptr<const AudioBuffers> audio)
{
	_sound_asset_writer->write (audio->data(), audio->frames());
}

struct QueueSorter
{
	bool operator() (pair<shared_ptr<const EncodedData>, int> const & a, pair<shared_ptr<const EncodedData>, int> const & b) {
		return a.second < b.second;
	}
};

void
Writer::thread ()
{
	while (1)
	{
		boost::mutex::scoped_lock lock (_mutex);

		while (1) {
			if (_finish ||
			    _queue.size() > _maximum_frames_in_memory ||
			    (!_queue.empty() && _queue.front().second == (_last_written_frame + 1))) {
				    
				    break;
			    }

			    TIMING ("writer sleeps with a queue of %1; %2 pending", _queue.size(), _pending.size());
			    _condition.wait (lock);
			    TIMING ("writer wakes with a queue of %1", _queue.size());

			    _queue.sort (QueueSorter ());
		}

		if (_finish && _queue.empty() && _pending.empty()) {
			return;
		}

		/* Write any frames that we can write; i.e. those that are in sequence */
		while (!_queue.empty() && _queue.front().second == (_last_written_frame + 1)) {
			pair<boost::shared_ptr<const EncodedData>, int> encoded = _queue.front ();
			_queue.pop_front ();

			lock.unlock ();
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

		while (_queue.size() > _maximum_frames_in_memory) {
			/* Too many frames in memory which can't yet be written to the stream.
			   Put some to disk.
			*/

			pair<boost::shared_ptr<const EncodedData>, int> encoded = _queue.back ();
			_queue.pop_back ();
			if (!encoded.first) {
				/* This is a `repeat-last' frame, so no need to write it to disk */
				continue;
			}

			lock.unlock ();
			_film->log()->log (String::compose ("Writer full (awaiting %1); pushes %2 to disk", _last_written_frame + 1, encoded.second));
			encoded.first->write (_film, encoded.second);
			lock.lock ();

			_pending.push_back (encoded.second);
		}

		while (_queue.size() < _maximum_frames_in_memory && !_pending.empty()) {
			/* We have some space in memory.  Fetch some frames back off disk. */

			_pending.sort ();
			int const fetch = _pending.front ();

			lock.unlock ();
			_film->log()->log (String::compose ("Writer pulls %1 back from disk", fetch));
			shared_ptr<const EncodedData> encoded;
			if (boost::filesystem::exists (_film->frame_out_path (fetch, false))) {
				/* It's an actual frame (not a repeat-last); load it in */
				encoded.reset (new EncodedData (_film->frame_out_path (fetch, false)));
			}
			lock.lock ();

			_queue.push_back (make_pair (encoded, fetch));
			_pending.remove (fetch);
		}
	}

}

void
Writer::finish ()
{
	if (!_thread) {
		return;
	}
	
	boost::mutex::scoped_lock lock (_mutex);
	_finish = true;
	_condition.notify_all ();
	lock.unlock ();

	_thread->join ();
	delete _thread;
	_thread = 0;

	_picture_asset_writer->finalize ();
	_sound_asset_writer->finalize ();


	int const frames = _film->dcp_intrinsic_duration().get();
	int const duration = frames - _film->trim_start() - _film->trim_end();
	
	_picture_asset->set_entry_point (_film->trim_start ());
	_picture_asset->set_duration (duration);
	_sound_asset->set_entry_point (_film->trim_start ());
	_sound_asset->set_duration (duration);
	
	libdcp::DCP dcp (_film->dir (_film->dcp_name()));
	DCPFrameRate dfr (_film->frames_per_second ());

	shared_ptr<libdcp::CPL> cpl (
		new libdcp::CPL (_film->dir (_film->dcp_name()), _film->dcp_name(), _film->dcp_content_type()->libdcp_kind (), frames, dfr.frames_per_second)
		);
	
	dcp.add_cpl (cpl);

	cpl->add_reel (shared_ptr<libdcp::Reel> (new libdcp::Reel (
							 _picture_asset,
							 _sound_asset,
							 shared_ptr<libdcp::SubtitleAsset> ()
							 )
			       ));

	dcp.write_xml ();
}

/** Tell the writer that frame `f' should be a repeat of the frame before it */
void
Writer::repeat (int f)
{
	boost::mutex::scoped_lock lock (_mutex);
	_queue.push_back (make_pair (shared_ptr<const EncodedData> (), f));
}
