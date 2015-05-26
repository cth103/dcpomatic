/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

#include "writer.h"
#include "compose.hpp"
#include "film.h"
#include "ratio.h"
#include "log.h"
#include "dcp_video.h"
#include "dcp_content_type.h"
#include "audio_mapping.h"
#include "config.h"
#include "job.h"
#include "cross.h"
#include "audio_buffers.h"
#include "md5_digester.h"
#include "encoded_data.h"
#include "version.h"
#include "font.h"
#include "util.h"
#include <dcp/mono_picture_mxf.h>
#include <dcp/stereo_picture_mxf.h>
#include <dcp/sound_mxf.h>
#include <dcp/sound_mxf_writer.h>
#include <dcp/reel.h>
#include <dcp/reel_mono_picture_asset.h>
#include <dcp/reel_stereo_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/reel_subtitle_asset.h>
#include <dcp/dcp.h>
#include <dcp/cpl.h>
#include <dcp/signer.h>
#include <dcp/interop_subtitle_content.h>
#include <dcp/font.h>
#include <boost/foreach.hpp>
#include <fstream>
#include <cerrno>

#include "i18n.h"

#define LOG_GENERAL(...) _film->log()->log (String::compose (__VA_ARGS__), Log::TYPE_GENERAL);
#define LOG_TIMING(...) _film->log()->microsecond_log (String::compose (__VA_ARGS__), Log::TYPE_TIMING);
#define LOG_WARNING_NC(...) _film->log()->log (__VA_ARGS__, Log::TYPE_WARNING);
#define LOG_WARNING(...) _film->log()->log (String::compose (__VA_ARGS__), Log::TYPE_WARNING);
#define LOG_ERROR(...) _film->log()->log (String::compose (__VA_ARGS__), Log::TYPE_ERROR);
#define LOG_DEBUG(...) _film->log()->log (String::compose (__VA_ARGS__), Log::TYPE_DEBUG);
#define LOG_DEBUG_NC(...) _film->log()->log (__VA_ARGS__, Log::TYPE_DEBUG);

/* OS X strikes again */
#undef set_key

using std::make_pair;
using std::pair;
using std::string;
using std::list;
using std::cout;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;

Writer::Writer (shared_ptr<const Film> f, weak_ptr<Job> j)
	: _film (f)
	, _job (j)
	, _first_nonexistant_frame (0)
	, _thread (0)
	, _finish (false)
	, _queued_full_in_memory (0)
	, _last_written_frame (-1)
	, _last_written_eyes (EYES_RIGHT)
	, _maximum_frames_in_memory (0)
	, _full_written (0)
	, _fake_written (0)
	, _pushed_to_disk (0)
{
	/* Remove any old DCP */
	boost::filesystem::remove_all (_film->dir (_film->dcp_name ()));

	shared_ptr<Job> job = _job.lock ();
	DCPOMATIC_ASSERT (job);

	/* Create our picture asset in a subdirectory, named according to those
	   film's parameters which affect the video output.  We will hard-link
	   it into the DCP later.
	*/

	if (_film->three_d ()) {
		_picture_mxf.reset (new dcp::StereoPictureMXF (dcp::Fraction (_film->video_frame_rate (), 1)));
	} else {
		_picture_mxf.reset (new dcp::MonoPictureMXF (dcp::Fraction (_film->video_frame_rate (), 1)));
	}

	_picture_mxf->set_size (_film->frame_size ());

	if (_film->encrypted ()) {
		_picture_mxf->set_key (_film->key ());
	}

	_picture_mxf->set_file (
		_film->internal_video_mxf_dir() / _film->internal_video_mxf_filename()
		);

	job->sub (_("Checking existing image data"));
	check_existing_picture_mxf ();
	
	_picture_mxf_writer = _picture_mxf->start_write (
		_film->internal_video_mxf_dir() / _film->internal_video_mxf_filename(),
		_film->interop() ? dcp::INTEROP : dcp::SMPTE,
		_first_nonexistant_frame > 0
		);

	if (_film->audio_channels ()) {
		_sound_mxf.reset (new dcp::SoundMXF (dcp::Fraction (_film->video_frame_rate(), 1), _film->audio_frame_rate (), _film->audio_channels ()));

		if (_film->encrypted ()) {
			_sound_mxf->set_key (_film->key ());
		}
	
		/* Write the sound MXF into the film directory so that we leave the creation
		   of the DCP directory until the last minute.
		*/
		_sound_mxf_writer = _sound_mxf->start_write (_film->directory() / audio_mxf_filename (_sound_mxf), _film->interop() ? dcp::INTEROP : dcp::SMPTE);
	}

	/* Check that the signer is OK if we need one */
	if (_film->is_signed() && !Config::instance()->signer()->valid ()) {
		throw InvalidSignerError ();
	}

	_thread = new boost::thread (boost::bind (&Writer::thread, this));

	job->sub (_("Encoding image data"));
}

Writer::~Writer ()
{
	terminate_thread (false);
}

void
Writer::write (shared_ptr<const EncodedData> encoded, int frame, Eyes eyes)
{
	boost::mutex::scoped_lock lock (_mutex);

	while (_queued_full_in_memory > _maximum_frames_in_memory) {
		/* The queue is too big; wait until that is sorted out */
		_full_condition.wait (lock);
	}

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

	/* Now there's something to do: wake anything wait()ing on _empty_condition */
	_empty_condition.notify_all ();
}

void
Writer::fake_write (int frame, Eyes eyes)
{
	boost::mutex::scoped_lock lock (_mutex);

	while (_queued_full_in_memory > _maximum_frames_in_memory) {
		/* The queue is too big; wait until that is sorted out */
		_full_condition.wait (lock);
	}
	
	FILE* file = fopen_boost (_film->info_file (), "rb");
	if (!file) {
		throw ReadFileError (_film->info_file ());
	}
	dcp::FrameInfo info = read_frame_info (file, frame, eyes);
	fclose (file);
	
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

	/* Now there's something to do: wake anything wait()ing on _empty_condition */
	_empty_condition.notify_all ();
}

/** This method is not thread safe */
void
Writer::write (shared_ptr<const AudioBuffers> audio)
{
	if (_sound_mxf_writer) {
		_sound_mxf_writer->write (audio->data(), audio->frames());
	}
}

/** This must be called from Writer::thread() with an appropriate lock held */
bool
Writer::have_sequenced_image_at_queue_head ()
{
	if (_queue.empty ()) {
		return false;
	}

	_queue.sort ();

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
	while (true)
	{
		boost::mutex::scoped_lock lock (_mutex);

		/* This is for debugging only */
		bool done_something = false;

		while (true) {
			
			if (_finish || _queued_full_in_memory > _maximum_frames_in_memory || have_sequenced_image_at_queue_head ()) {
				/* We've got something to do: go and do it */
				break;
			}

			/* Nothing to do: wait until something happens which may indicate that we do */
			LOG_TIMING (N_("writer sleeps with a queue of %1"), _queue.size());
			_empty_condition.wait (lock);
			LOG_TIMING (N_("writer wakes with a queue of %1"), _queue.size());
		}

		if (_finish && _queue.empty()) {
			return;
		}

		/* We stop here if we have been asked to finish, and if either the queue
		   is empty or we do not have a sequenced image at its head (if this is the
		   case we will never terminate as no new frames will be sent once
		   _finish is true).
		*/
		if (_finish && (!have_sequenced_image_at_queue_head() || _queue.empty())) {
			done_something = true;
			/* (Hopefully temporarily) log anything that was not written */
			if (!_queue.empty() && !have_sequenced_image_at_queue_head()) {
				LOG_WARNING (N_("Finishing writer with a left-over queue of %1:"), _queue.size());
				for (list<QueueItem>::const_iterator i = _queue.begin(); i != _queue.end(); ++i) {
					if (i->type == QueueItem::FULL) {
						LOG_WARNING (N_("- type FULL, frame %1, eyes %2"), i->frame, i->eyes);
					} else {
						LOG_WARNING (N_("- type FAKE, size %1, frame %2, eyes %3"), i->size, i->frame, i->eyes);
					}						
				}
				LOG_WARNING (N_("Last written frame %1, last written eyes %2"), _last_written_frame, _last_written_eyes);
			}
			return;
		}
		/* Write any frames that we can write; i.e. those that are in sequence. */
		while (have_sequenced_image_at_queue_head ()) {
			done_something = true;
			QueueItem qi = _queue.front ();
			_queue.pop_front ();
			if (qi.type == QueueItem::FULL && qi.encoded) {
				--_queued_full_in_memory;
			}

			lock.unlock ();
			switch (qi.type) {
			case QueueItem::FULL:
			{
				LOG_GENERAL (N_("Writer FULL-writes %1 (%2) to MXF"), qi.frame, qi.eyes);
				if (!qi.encoded) {
					qi.encoded.reset (new EncodedData (_film->j2c_path (qi.frame, qi.eyes, false)));
				}

				dcp::FrameInfo fin = _picture_mxf_writer->write (qi.encoded->data(), qi.encoded->size());
				qi.encoded->write_info (_film, qi.frame, qi.eyes, fin);
				_last_written[qi.eyes] = qi.encoded;
				++_full_written;
				break;
			}
			case QueueItem::FAKE:
				LOG_GENERAL (N_("Writer FAKE-writes %1 to MXF"), qi.frame);
				_picture_mxf_writer->fake_write (qi.size);
				_last_written[qi.eyes].reset ();
				++_fake_written;
				break;
			}
			lock.lock ();

			_last_written_frame = qi.frame;
			_last_written_eyes = qi.eyes;
			
			shared_ptr<Job> job = _job.lock ();
			DCPOMATIC_ASSERT (job);
			int64_t total = _film->length().frames (_film->video_frame_rate ());
			if (_film->three_d ()) {
				/* _full_written and so on are incremented for each eye, so we need to double the total
				   frames to get the correct progress.
				*/
				total *= 2;
			}
			if (total) {
				job->set_progress (float (_full_written + _fake_written) / total);
			}
		}

		while (_queued_full_in_memory > _maximum_frames_in_memory) {
			done_something = true;
			/* Too many frames in memory which can't yet be written to the stream.
			   Write some FULL frames to disk.
			*/

			/* Find one from the back of the queue */
			_queue.sort ();
			list<QueueItem>::reverse_iterator i = _queue.rbegin ();
			while (i != _queue.rend() && (i->type != QueueItem::FULL || !i->encoded)) {
				++i;
			}

			DCPOMATIC_ASSERT (i != _queue.rend());
			++_pushed_to_disk;
			lock.unlock ();

			/* i is valid here, even though we don't hold a lock on the mutex,
			   since list iterators are unaffected by insertion and only this
			   thread could erase the last item in the list.
			*/

			LOG_GENERAL (
				"Writer full (awaiting %1 [last eye was %2]); pushes %3 to disk",
				_last_written_frame + 1,
				_last_written_eyes, i->frame
				);
			
			i->encoded->write (_film, i->frame, i->eyes);
			
			lock.lock ();
			i->encoded.reset ();
			--_queued_full_in_memory;
		}

		if (!done_something) {
			LOG_DEBUG_NC ("Writer loop ran without doing anything");
			LOG_DEBUG ("_queued_full_in_memory=%1", _queued_full_in_memory);
			LOG_DEBUG ("_queue_size=%1", _queue.size ());
			LOG_DEBUG ("_finish=%1", _finish);
			LOG_DEBUG ("_last_written_frame=%1", _last_written_frame);
		}

		/* The queue has probably just gone down a bit; notify anything wait()ing on _full_condition */
		_full_condition.notify_all ();
	}
}
catch (...)
{
	store_current ();
}

void
Writer::terminate_thread (bool can_throw)
{
	boost::mutex::scoped_lock lock (_mutex);
	if (_thread == 0) {
		return;
	}
	
	_finish = true;
	_empty_condition.notify_all ();
	_full_condition.notify_all ();
	lock.unlock ();

 	_thread->join ();
	if (can_throw) {
		rethrow ();
	}
	
	delete _thread;
	_thread = 0;
}	

void
Writer::finish ()
{
	if (!_thread) {
		return;
	}
	
	terminate_thread (true);

	_picture_mxf_writer->finalize ();
	if (_sound_mxf_writer) {
		_sound_mxf_writer->finalize ();
	}
	
	/* Hard-link the video MXF into the DCP */
	boost::filesystem::path video_from = _picture_mxf->file ();
	
	boost::filesystem::path video_to;
	video_to /= _film->dir (_film->dcp_name());
	video_to /= video_mxf_filename (_picture_mxf);

	boost::system::error_code ec;
	boost::filesystem::create_hard_link (video_from, video_to, ec);
	if (ec) {
		LOG_WARNING_NC ("Hard-link failed; copying instead");
		boost::filesystem::copy_file (video_from, video_to, ec);
		if (ec) {
			LOG_ERROR ("Failed to copy video file from %1 to %2 (%3)", video_from.string(), video_to.string(), ec.message ());
			throw FileError (ec.message(), video_from);
		}
	}

	_picture_mxf->set_file (video_to);

	/* Move the audio MXF into the DCP */

	if (_sound_mxf) {
		boost::filesystem::path audio_to;
		audio_to /= _film->dir (_film->dcp_name ());
		audio_to /= audio_mxf_filename (_sound_mxf);
		
		boost::filesystem::rename (_film->file (audio_mxf_filename (_sound_mxf)), audio_to, ec);
		if (ec) {
			throw FileError (
				String::compose (_("could not move audio MXF into the DCP (%1)"), ec.value ()), audio_mxf_filename (_sound_mxf)
				);
		}

		_sound_mxf->set_file (audio_to);
	}

	dcp::DCP dcp (_film->dir (_film->dcp_name()));

	shared_ptr<dcp::CPL> cpl (
		new dcp::CPL (
			_film->dcp_name(),
			_film->dcp_content_type()->libdcp_kind ()
			)
		);
	
	dcp.add (cpl);

	shared_ptr<dcp::Reel> reel (new dcp::Reel ());

	shared_ptr<dcp::MonoPictureMXF> mono = dynamic_pointer_cast<dcp::MonoPictureMXF> (_picture_mxf);
	if (mono) {
		reel->add (shared_ptr<dcp::ReelPictureAsset> (new dcp::ReelMonoPictureAsset (mono, 0)));
		dcp.add (mono);
	}

	shared_ptr<dcp::StereoPictureMXF> stereo = dynamic_pointer_cast<dcp::StereoPictureMXF> (_picture_mxf);
	if (stereo) {
		reel->add (shared_ptr<dcp::ReelPictureAsset> (new dcp::ReelStereoPictureAsset (stereo, 0)));
		dcp.add (stereo);
	}

	if (_sound_mxf) {
		reel->add (shared_ptr<dcp::ReelSoundAsset> (new dcp::ReelSoundAsset (_sound_mxf, 0)));
		dcp.add (_sound_mxf);
	}

	if (_subtitle_content) {
		boost::filesystem::path const liberation = shared_path () / "LiberationSans-Regular.ttf";

		/* Add all the fonts to the subtitle content and as assets to the DCP */
		BOOST_FOREACH (shared_ptr<Font> i, _fonts) {
			boost::filesystem::path const from = i->file.get_value_or (liberation);
			_subtitle_content->add_font (i->id, from.leaf().string ());

			boost::filesystem::path to = _film->dir (_film->dcp_name ()) / _subtitle_content->id ();
			boost::filesystem::create_directories (to, ec);
			if (ec) {
				throw FileError (_("Could not create directory"), to);
			}

			to /= from.leaf();

			boost::system::error_code ec;
			boost::filesystem::copy_file (from, to, ec);
			if (ec) {
				throw FileError ("Could not copy font to DCP", from);
			}

			dcp.add (shared_ptr<dcp::Font> (new dcp::Font (to)));
		}

		_subtitle_content->write_xml (_film->dir (_film->dcp_name ()) / _subtitle_content->id () / subtitle_content_filename (_subtitle_content));
		reel->add (shared_ptr<dcp::ReelSubtitleAsset> (
				   new dcp::ReelSubtitleAsset (
					   _subtitle_content,
					   dcp::Fraction (_film->video_frame_rate(), 1),
					   _picture_mxf->intrinsic_duration (),
					   0
					   )
				   ));
		
		dcp.add (_subtitle_content);
	}
	
	cpl->add (reel);

	shared_ptr<Job> job = _job.lock ();
	DCPOMATIC_ASSERT (job);

	job->sub (_("Computing image digest"));
	_picture_mxf->hash (boost::bind (&Job::set_progress, job.get(), _1, false));

	if (_sound_mxf) {
		job->sub (_("Computing audio digest"));
		_sound_mxf->hash (boost::bind (&Job::set_progress, job.get(), _1, false));
	}

	dcp::XMLMetadata meta;
	meta.issuer = Config::instance()->dcp_issuer ();
	meta.creator = String::compose ("DCP-o-matic %1 %2", dcpomatic_version, dcpomatic_git_commit);
	meta.set_issue_date_now ();

	shared_ptr<const dcp::Signer> signer;
	if (_film->is_signed ()) {
		signer = Config::instance()->signer ();
		/* We did check earlier, but check again here to be on the safe side */
		if (!signer->valid ()) {
			throw InvalidSignerError ();
		}
	}

	dcp.write_xml (_film->interop () ? dcp::INTEROP : dcp::SMPTE, meta, signer);

	LOG_GENERAL (
		N_("Wrote %1 FULL, %2 FAKE, %3 pushed to disk"), _full_written, _fake_written, _pushed_to_disk
		);
}

bool
Writer::check_existing_picture_mxf_frame (FILE* mxf, int f, Eyes eyes)
{
	/* Read the frame info as written */
	FILE* file = fopen_boost (_film->info_file (), "rb");
	if (!file) {
		LOG_GENERAL ("Existing frame %1 has no info file", f);
		return false;
	}
	
	dcp::FrameInfo info = read_frame_info (file, f, eyes);
	fclose (file);
	if (info.size == 0) {
		LOG_GENERAL ("Existing frame %1 has no info file", f);
		return false;
	}
	
	/* Read the data from the MXF and hash it */
	dcpomatic_fseek (mxf, info.offset, SEEK_SET);
	EncodedData data (info.size);
	size_t const read = fread (data.data(), 1, data.size(), mxf);
	if (read != static_cast<size_t> (data.size ())) {
		LOG_GENERAL ("Existing frame %1 is incomplete", f);
		return false;
	}

	MD5Digester digester;
	digester.add (data.data(), data.size());
	if (digester.get() != info.hash) {
		LOG_GENERAL ("Existing frame %1 failed hash check", f);
		return false;
	}

	return true;
}

void
Writer::check_existing_picture_mxf ()
{
	/* Try to open the existing MXF */
	FILE* mxf = fopen_boost (_picture_mxf->file(), "rb");
	if (!mxf) {
		LOG_GENERAL ("Could not open existing MXF at %1 (errno=%2)", _picture_mxf->file().string(), errno);
		return;
	}

	while (true) {

		shared_ptr<Job> job = _job.lock ();
		DCPOMATIC_ASSERT (job);

		job->set_progress_unknown ();

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

		LOG_GENERAL ("Have existing frame %1", _first_nonexistant_frame);
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

void
Writer::write (PlayerSubtitles subs)
{
	if (subs.text.empty ()) {
		return;
	}

	if (!_subtitle_content) {
		string lang = _film->subtitle_language ();
		if (lang.empty ()) {
			lang = "Unknown";
		}
		_subtitle_content.reset (new dcp::InteropSubtitleContent (_film->name(), lang));
	}
	
	for (list<dcp::SubtitleString>::const_iterator i = subs.text.begin(); i != subs.text.end(); ++i) {
		_subtitle_content->add (*i);
	}
}

void
Writer::write (list<shared_ptr<Font> > fonts)
{
	/* Just keep a list of fonts and we'll deal with them in ::finish */
	copy (fonts.begin (), fonts.end (), back_inserter (_fonts));
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

void
Writer::set_encoder_threads (int threads)
{
	_maximum_frames_in_memory = rint (threads * 1.1);
}
