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

#include <fstream>
#include <cerrno>
#include <libdcp/picture_asset.h>
#include <libdcp/sound_asset.h>
#include <libdcp/picture_frame.h>
#include <libdcp/reel.h>
#include <libdcp/dcp.h>
#include <libdcp/cpl.h>
#include "writer.h"
#include "compose.hpp"
#include "film.h"
#include "ratio.h"
#include "log.h"
#include "dcp_video_frame.h"
#include "dcp_content_type.h"
#include "player.h"
#include "audio_mapping.h"
#include "config.h"
#include "job.h"

#include "i18n.h"

using std::make_pair;
using std::pair;
using std::string;
using std::ifstream;
using std::list;
using std::cout;
using boost::shared_ptr;

int const Writer::_maximum_frames_in_memory = 8;

Writer::Writer (shared_ptr<const Film> f, shared_ptr<Job> j)
	: _film (f)
	, _job (j)
	, _first_nonexistant_frame (0)
	, _thread (0)
	, _finish (false)
	, _queued_full_in_memory (0)
	, _last_written_frame (-1)
	, _last_written_eyes (EYES_RIGHT)
	, _full_written (0)
	, _fake_written (0)
	, _repeat_written (0)
	, _pushed_to_disk (0)
{
	/* Remove any old DCP */
	boost::filesystem::remove_all (_film->dir (_film->dcp_name ()));
	
	check_existing_picture_mxf ();
	
	/* Create our picture asset in a subdirectory, named according to those
	   film's parameters which affect the video output.  We will hard-link
	   it into the DCP later.
	*/

	if (f->three_d ()) {
		_picture_asset.reset (
			new libdcp::StereoPictureAsset (
				_film->internal_video_mxf_dir (),
				_film->internal_video_mxf_filename (),
				_film->video_frame_rate (),
				_film->container()->size (_film->full_frame ())
				)
			);
		
	} else {
		_picture_asset.reset (
			new libdcp::MonoPictureAsset (
				_film->internal_video_mxf_dir (),
				_film->internal_video_mxf_filename (),
				_film->video_frame_rate (),
				_film->container()->size (_film->full_frame ())
				)
			);

	}

	_picture_asset_writer = _picture_asset->start_write (_first_nonexistant_frame > 0, _film->interop ());
	
	_sound_asset.reset (
		new libdcp::SoundAsset (
			_film->dir (_film->dcp_name()),
			_film->audio_mxf_filename (),
			_film->video_frame_rate (),
			_film->audio_channels (),
			_film->audio_frame_rate ()
			)
		);
	
	_sound_asset_writer = _sound_asset->start_write (_film->interop ());

	_thread = new boost::thread (boost::bind (&Writer::thread, this));
}

void
Writer::write (shared_ptr<const EncodedData> encoded, int frame, Eyes eyes)
{
	boost::mutex::scoped_lock lock (_mutex);

	QueueItem qi;
	qi.type = QueueItem::FULL;
	qi.encoded = encoded;
	qi.frame = frame;

	if (_film->three_d() && eyes == EYES_BOTH) {
		/* 2D material in a 3D DCP; fake the 3D */
		qi.eyes = EYES_LEFT;
		_queue.push_back (qi);
		++_queued_full_in_memory;
		qi.eyes = EYES_RIGHT;
		_queue.push_back (qi);
		++_queued_full_in_memory;
	} else {
		qi.eyes = eyes;
		_queue.push_back (qi);
		++_queued_full_in_memory;
	}
	
	_condition.notify_all ();
}

void
Writer::fake_write (int frame, Eyes eyes)
{
	boost::mutex::scoped_lock lock (_mutex);

	ifstream ifi (_film->info_path (frame, eyes).c_str());
	libdcp::FrameInfo info (ifi);
	
	QueueItem qi;
	qi.type = QueueItem::FAKE;
	qi.size = info.size;
	qi.frame = frame;
	if (_film->three_d() && eyes == EYES_BOTH) {
		qi.eyes = EYES_LEFT;
		_queue.push_back (qi);
		qi.eyes = EYES_RIGHT;
		_queue.push_back (qi);
	} else {
		qi.eyes = eyes;
		_queue.push_back (qi);
	}

	_condition.notify_all ();
}

/** This method is not thread safe */
void
Writer::write (shared_ptr<const AudioBuffers> audio)
{
	_sound_asset_writer->write (audio->data(), audio->frames());
}

/** This must be called from Writer::thread() with an appropriate lock held,
 *  and with _queue sorted.
 */
bool
Writer::have_sequenced_image_at_queue_head () const
{
	if (_queue.empty ()) {
		return false;
	}

	/* The queue should contain only EYES_LEFT/EYES_RIGHT pairs or EYES_BOTH */

	if (_queue.front().eyes == EYES_BOTH) {
		/* 2D */
		return _queue.front().frame == (_last_written_frame + 1);
	}

	/* 3D */

	if (_last_written_eyes == EYES_LEFT && _queue.front().frame == _last_written_frame && _queue.front().eyes == EYES_RIGHT) {
		return true;
	}

	if (_last_written_eyes == EYES_RIGHT && _queue.front().frame == (_last_written_frame + 1) && _queue.front().eyes == EYES_LEFT) {
		return true;
	}

	return false;
}

void
Writer::thread ()
try
{
	while (1)
	{
		boost::mutex::scoped_lock lock (_mutex);

		while (1) {
			
			_queue.sort ();
			
			if (_finish || _queued_full_in_memory > _maximum_frames_in_memory || have_sequenced_image_at_queue_head ()) {
				break;
			}

			TIMING (N_("writer sleeps with a queue of %1"), _queue.size());
			_condition.wait (lock);
			TIMING (N_("writer wakes with a queue of %1"), _queue.size());
		}

		if (_finish && _queue.empty()) {
			return;
		}

		/* Write any frames that we can write; i.e. those that are in sequence */
		while (have_sequenced_image_at_queue_head ()) {
			QueueItem qi = _queue.front ();
			_queue.pop_front ();
			if (qi.type == QueueItem::FULL && qi.encoded) {
				--_queued_full_in_memory;
			}

			lock.unlock ();
			switch (qi.type) {
			case QueueItem::FULL:
			{
				_film->log()->log (String::compose (N_("Writer FULL-writes %1 to MXF"), qi.frame));
				if (!qi.encoded) {
					qi.encoded.reset (new EncodedData (_film->j2c_path (qi.frame, qi.eyes, false)));
				}

				libdcp::FrameInfo fin = _picture_asset_writer->write (qi.encoded->data(), qi.encoded->size());
				qi.encoded->write_info (_film, qi.frame, qi.eyes, fin);
				_last_written[qi.eyes] = qi.encoded;
				++_full_written;
				break;
			}
			case QueueItem::FAKE:
				_film->log()->log (String::compose (N_("Writer FAKE-writes %1 to MXF"), qi.frame));
				_picture_asset_writer->fake_write (qi.size);
				_last_written[qi.eyes].reset ();
				++_fake_written;
				break;
			case QueueItem::REPEAT:
			{
				_film->log()->log (String::compose (N_("Writer REPEAT-writes %1 to MXF"), qi.frame));
				libdcp::FrameInfo fin = _picture_asset_writer->write (
					_last_written[qi.eyes]->data(),
					_last_written[qi.eyes]->size()
					);
				
				_last_written[qi.eyes]->write_info (_film, qi.frame, qi.eyes, fin);
				++_repeat_written;
				break;
			}
			}
			lock.lock ();

			_last_written_frame = qi.frame;
			_last_written_eyes = qi.eyes;
			
			if (_film->length()) {
				_job->set_progress (
					float (_full_written + _fake_written + _repeat_written) / _film->time_to_video_frames (_film->length())
					);
			}
		}

		while (_queued_full_in_memory > _maximum_frames_in_memory) {
			/* Too many frames in memory which can't yet be written to the stream.
			   Write some FULL frames to disk.
			*/

			/* Find one */
			list<QueueItem>::reverse_iterator i = _queue.rbegin ();
			while (i != _queue.rend() && (i->type != QueueItem::FULL || !i->encoded)) {
				++i;
			}

			assert (i != _queue.rend());
			QueueItem qi = *i;

			++_pushed_to_disk;
			
			lock.unlock ();

			_film->log()->log (
				String::compose (
					"Writer full (awaiting %1 [last eye was %2]); pushes %3 to disk",
					_last_written_frame + 1,
					_last_written_eyes, qi.frame)
				);
			
			qi.encoded->write (_film, qi.frame, qi.eyes);
			lock.lock ();
			qi.encoded.reset ();
			--_queued_full_in_memory;
		}
	}
}
catch (...)
{
	store_current ();
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
	if (thrown ()) {
		rethrow ();
	}
	
	delete _thread;
	_thread = 0;

	_picture_asset_writer->finalize ();
	_sound_asset_writer->finalize ();
	
	int const frames = _last_written_frame + 1;

	_picture_asset->set_duration (frames);

	/* Hard-link the video MXF into the DCP */

	boost::filesystem::path from;
	from /= _film->internal_video_mxf_dir();
	from /= _film->internal_video_mxf_filename();
	
	boost::filesystem::path to;
	to /= _film->dir (_film->dcp_name());
	to /= _film->video_mxf_filename ();

	boost::system::error_code ec;
	boost::filesystem::create_hard_link (from, to, ec);
	if (ec) {
		/* hard link failed; copy instead */
		boost::filesystem::copy_file (from, to);
		_film->log()->log ("Hard-link failed; fell back to copying");
	}

	/* And update the asset */

	_picture_asset->set_directory (_film->dir (_film->dcp_name ()));
	_picture_asset->set_file_name (_film->video_mxf_filename ());
	_sound_asset->set_duration (frames);
	
	libdcp::DCP dcp (_film->dir (_film->dcp_name()));

	shared_ptr<libdcp::CPL> cpl (
		new libdcp::CPL (
			_film->dir (_film->dcp_name()),
			_film->dcp_name(),
			_film->dcp_content_type()->libdcp_kind (),
			frames,
			_film->video_frame_rate ()
			)
		);
	
	dcp.add_cpl (cpl);

	cpl->add_reel (shared_ptr<libdcp::Reel> (new libdcp::Reel (
							 _picture_asset,
							 _sound_asset,
							 shared_ptr<libdcp::SubtitleAsset> ()
							 )
			       ));

	libdcp::XMLMetadata meta = Config::instance()->dcp_metadata ();
	meta.set_issue_date_now ();
	dcp.write_xml (_film->interop (), meta);

	_film->log()->log (String::compose (N_("Wrote %1 FULL, %2 FAKE, %3 REPEAT; %4 pushed to disk"), _full_written, _fake_written, _repeat_written, _pushed_to_disk));
}

/** Tell the writer that frame `f' should be a repeat of the frame before it */
void
Writer::repeat (int f, Eyes e)
{
	boost::mutex::scoped_lock lock (_mutex);

	QueueItem qi;
	qi.type = QueueItem::REPEAT;
	qi.frame = f;
	if (_film->three_d() && e == EYES_BOTH) {
		qi.eyes = EYES_LEFT;
		_queue.push_back (qi);
		qi.eyes = EYES_RIGHT;
		_queue.push_back (qi);
	} else {
		qi.eyes = e;
		_queue.push_back (qi);
	}

	_condition.notify_all ();
}

bool
Writer::check_existing_picture_mxf_frame (FILE* mxf, int f, Eyes eyes)
{
	/* Read the frame info as written */
	ifstream ifi (_film->info_path (f, eyes).c_str());
	libdcp::FrameInfo info (ifi);
	if (info.size == 0) {
		_film->log()->log (String::compose ("Existing frame %1 has no info file", f));
		return false;
	}
	
	/* Read the data from the MXF and hash it */
	fseek (mxf, info.offset, SEEK_SET);
	EncodedData data (info.size);
	size_t const read = fread (data.data(), 1, data.size(), mxf);
	if (read != static_cast<size_t> (data.size ())) {
		_film->log()->log (String::compose ("Existing frame %1 is incomplete", f));
		return false;
	}
	
	string const existing_hash = md5_digest (data.data(), data.size());
	if (existing_hash != info.hash) {
		_film->log()->log (String::compose ("Existing frame %1 failed hash check", f));
		return false;
	}

	return true;
}

void
Writer::check_existing_picture_mxf ()
{
	/* Try to open the existing MXF */
	boost::filesystem::path p;
	p /= _film->internal_video_mxf_dir ();
	p /= _film->internal_video_mxf_filename ();
	FILE* mxf = fopen (p.string().c_str(), "rb");
	if (!mxf) {
		_film->log()->log (String::compose ("Could not open existing MXF at %1 (errno=%2)", p.string(), errno));
		return;
	}

	while (1) {

		if (_film->three_d ()) {
			if (!check_existing_picture_mxf_frame (mxf, _first_nonexistant_frame, EYES_LEFT)) {
				break;
			}
			if (!check_existing_picture_mxf_frame (mxf, _first_nonexistant_frame, EYES_RIGHT)) {
				break;
			}
		} else {
			if (!check_existing_picture_mxf_frame (mxf, _first_nonexistant_frame, EYES_BOTH)) {
				break;
			}
		}

		_film->log()->log (String::compose ("Have existing frame %1", _first_nonexistant_frame));
		++_first_nonexistant_frame;
	}

	fclose (mxf);
}

/** @param frame Frame index.
 *  @return true if we can fake-write this frame.
 */
bool
Writer::can_fake_write (int frame) const
{
	/* We have to do a proper write of the first frame so that we can set up the JPEG2000
	   parameters in the MXF writer.
	*/
	return (frame != 0 && frame < _first_nonexistant_frame);
}

bool
operator< (QueueItem const & a, QueueItem const & b)
{
	if (a.frame != b.frame) {
		return a.frame < b.frame;
	}

	return static_cast<int> (a.eyes) < static_cast<int> (b.eyes);
}

bool
operator== (QueueItem const & a, QueueItem const & b)
{
	return a.frame == b.frame && a.eyes == b.eyes;
}
