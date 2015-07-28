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
#include "data.h"
#include "version.h"
#include "font.h"
#include "util.h"
#include <dcp/mono_picture_asset.h>
#include <dcp/stereo_picture_asset.h>
#include <dcp/sound_asset.h>
#include <dcp/sound_asset_writer.h>
#include <dcp/reel.h>
#include <dcp/reel_mono_picture_asset.h>
#include <dcp/reel_stereo_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/reel_subtitle_asset.h>
#include <dcp/dcp.h>
#include <dcp/cpl.h>
#include <dcp/signer.h>
#include <dcp/interop_subtitle_asset.h>
#include <dcp/smpte_subtitle_asset.h>
#include <boost/foreach.hpp>
#include <fstream>
#include <cerrno>

#include "i18n.h"

#define LOG_GENERAL(...) _film->log()->log (String::compose (__VA_ARGS__), Log::TYPE_GENERAL);
#define LOG_DEBUG_ENCODE(...) _film->log()->log (String::compose (__VA_ARGS__), Log::TYPE_DEBUG_ENCODE);
#define LOG_TIMING(...) _film->log()->microsecond_log (String::compose (__VA_ARGS__), Log::TYPE_TIMING);
#define LOG_WARNING_NC(...) _film->log()->log (__VA_ARGS__, Log::TYPE_WARNING);
#define LOG_WARNING(...) _film->log()->log (String::compose (__VA_ARGS__), Log::TYPE_WARNING);
#define LOG_ERROR(...) _film->log()->log (String::compose (__VA_ARGS__), Log::TYPE_ERROR);

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

Writer::Writer (shared_ptr<const Film> film, weak_ptr<Job> j)
	: _film (film)
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
	, _repeat_written (0)
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
		_picture_asset.reset (new dcp::StereoPictureAsset (dcp::Fraction (_film->video_frame_rate (), 1)));
	} else {
		_picture_asset.reset (new dcp::MonoPictureAsset (dcp::Fraction (_film->video_frame_rate (), 1)));
	}

	_picture_asset->set_size (_film->frame_size ());

	if (_film->encrypted ()) {
		_picture_asset->set_key (_film->key ());
	}

	_picture_asset->set_file (
		_film->internal_video_asset_dir() / _film->internal_video_asset_filename()
		);

	job->sub (_("Checking existing image data"));
	check_existing_picture_asset ();

	_picture_asset_writer = _picture_asset->start_write (
		_film->internal_video_asset_dir() / _film->internal_video_asset_filename(),
		_film->interop() ? dcp::INTEROP : dcp::SMPTE,
		_first_nonexistant_frame > 0
		);

	if (_film->audio_channels ()) {
		_sound_asset.reset (
			new dcp::SoundAsset (dcp::Fraction (_film->video_frame_rate(), 1), _film->audio_frame_rate (), _film->audio_channels ())
			);

		if (_film->encrypted ()) {
			_sound_asset->set_key (_film->key ());
		}

		/* Write the sound asset into the film directory so that we leave the creation
		   of the DCP directory until the last minute.
		*/
		_sound_asset_writer = _sound_asset->start_write (
			_film->directory() / audio_asset_filename (_sound_asset),
			_film->interop() ? dcp::INTEROP : dcp::SMPTE
			);
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
Writer::write (Data encoded, int frame, Eyes eyes)
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
Writer::repeat (int frame, Eyes eyes)
{
	boost::mutex::scoped_lock lock (_mutex);

	while (_queued_full_in_memory > _maximum_frames_in_memory) {
		/* The queue is too big; wait until that is sorted out */
		_full_condition.wait (lock);
	}

	QueueItem qi;
	qi.type = QueueItem::REPEAT;
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
	if (_sound_asset_writer) {
		_sound_asset_writer->write (audio->data(), audio->frames());
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
Writer::write_frame_info (int frame, Eyes eyes, dcp::FrameInfo info) const
{
	FILE* file = fopen_boost (_film->info_file(), "ab");
	if (!file) {
		throw OpenFileError (_film->info_file ());
	}
	dcpomatic_fseek (file, frame_info_position (frame, eyes), SEEK_SET);
	fwrite (&info.offset, sizeof (info.offset), 1, file);
	fwrite (&info.size, sizeof (info.size), 1, file);
	fwrite (info.hash.c_str(), 1, info.hash.size(), file);
	fclose (file);
}

void
Writer::thread ()
try
{
	while (true)
	{
		boost::mutex::scoped_lock lock (_mutex);

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
			QueueItem qi = _queue.front ();
			_queue.pop_front ();
			if (qi.type == QueueItem::FULL && qi.encoded) {
				--_queued_full_in_memory;
			}

			lock.unlock ();
			switch (qi.type) {
			case QueueItem::FULL:
			{
				LOG_DEBUG_ENCODE (N_("Writer FULL-writes %1 (%2)"), qi.frame, qi.eyes);
				if (!qi.encoded) {
					qi.encoded = Data (_film->j2c_path (qi.frame, qi.eyes, false));
				}

				dcp::FrameInfo fin = _picture_asset_writer->write (qi.encoded->data().get (), qi.encoded->size());
				write_frame_info (qi.frame, qi.eyes, fin);
				_last_written[qi.eyes] = qi.encoded;
				++_full_written;
				break;
			}
			case QueueItem::FAKE:
				LOG_DEBUG_ENCODE (N_("Writer FAKE-writes %1"), qi.frame);
				_picture_asset_writer->fake_write (qi.size);
				_last_written[qi.eyes].reset ();
				++_fake_written;
				break;
			case QueueItem::REPEAT:
				LOG_DEBUG_ENCODE (N_("Writer REPEAT-writes %1"), qi.frame);
				dcp::FrameInfo fin = _picture_asset_writer->write (
					_last_written[qi.eyes]->data().get(),
					_last_written[qi.eyes]->size()
					);
				write_frame_info (qi.frame, qi.eyes, fin);
				++_repeat_written;
				break;
			}
			lock.lock ();

			_last_written_frame = qi.frame;
			_last_written_eyes = qi.eyes;

			shared_ptr<Job> job = _job.lock ();
			DCPOMATIC_ASSERT (job);
			int64_t total = _film->length().frames_round (_film->video_frame_rate ());
			if (_film->three_d ()) {
				/* _full_written and so on are incremented for each eye, so we need to double the total
				   frames to get the correct progress.
				*/
				total *= 2;
			}
			if (total) {
				job->set_progress (float (_full_written + _fake_written + _repeat_written) / total);
			}
		}

		while (_queued_full_in_memory > _maximum_frames_in_memory) {
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

			i->encoded->write_via_temp (_film->j2c_path (i->frame, i->eyes, true), _film->j2c_path (i->frame, i->eyes, false));

			lock.lock ();
			i->encoded.reset ();
			--_queued_full_in_memory;
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

	_picture_asset_writer->finalize ();
	if (_sound_asset_writer) {
		_sound_asset_writer->finalize ();
	}

	/* Hard-link the video asset into the DCP */
	boost::filesystem::path video_from = _picture_asset->file ();

	boost::filesystem::path video_to;
	video_to /= _film->dir (_film->dcp_name());
	video_to /= video_asset_filename (_picture_asset);

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

	_picture_asset->set_file (video_to);

	/* Move the audio asset into the DCP */

	if (_sound_asset) {
		boost::filesystem::path audio_to;
		audio_to /= _film->dir (_film->dcp_name ());
		audio_to /= audio_asset_filename (_sound_asset);

		boost::filesystem::rename (_film->file (audio_asset_filename (_sound_asset)), audio_to, ec);
		if (ec) {
			throw FileError (
				String::compose (_("could not move audio asset into the DCP (%1)"), ec.value ()), audio_asset_filename (_sound_asset)
				);
		}

		_sound_asset->set_file (audio_to);
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

	shared_ptr<dcp::MonoPictureAsset> mono = dynamic_pointer_cast<dcp::MonoPictureAsset> (_picture_asset);
	if (mono) {
		reel->add (shared_ptr<dcp::ReelPictureAsset> (new dcp::ReelMonoPictureAsset (mono, 0)));
	}

	shared_ptr<dcp::StereoPictureAsset> stereo = dynamic_pointer_cast<dcp::StereoPictureAsset> (_picture_asset);
	if (stereo) {
		reel->add (shared_ptr<dcp::ReelPictureAsset> (new dcp::ReelStereoPictureAsset (stereo, 0)));
	}

	if (_sound_asset) {
		reel->add (shared_ptr<dcp::ReelSoundAsset> (new dcp::ReelSoundAsset (_sound_asset, 0)));
	}

	if (_subtitle_asset) {
		boost::filesystem::path const liberation = shared_path () / "LiberationSans-Regular.ttf";

		/* Add all the fonts to the subtitle content */
		BOOST_FOREACH (shared_ptr<Font> i, _fonts) {
			_subtitle_asset->add_font (i->id(), i->file().get_value_or (liberation));
		}

		if (dynamic_pointer_cast<dcp::InteropSubtitleAsset> (_subtitle_asset)) {
			boost::filesystem::path directory = _film->dir (_film->dcp_name ()) / _subtitle_asset->id ();
			boost::filesystem::create_directories (directory);
			_subtitle_asset->write (directory / ("sub_" + _subtitle_asset->id() + ".xml"));
		} else {
			_subtitle_asset->write (
				_film->dir (_film->dcp_name ()) / ("sub_" + _subtitle_asset->id() + ".mxf")
				);
		}

		reel->add (shared_ptr<dcp::ReelSubtitleAsset> (
				   new dcp::ReelSubtitleAsset (
					   _subtitle_asset,
					   dcp::Fraction (_film->video_frame_rate(), 1),
					   _picture_asset->intrinsic_duration (),
					   0
					   )
				   ));
	}

	cpl->add (reel);

	shared_ptr<Job> job = _job.lock ();
	DCPOMATIC_ASSERT (job);

	job->sub (_("Computing image digest"));
	_picture_asset->hash (boost::bind (&Job::set_progress, job.get(), _1, false));

	if (_sound_asset) {
		job->sub (_("Computing audio digest"));
		_sound_asset->hash (boost::bind (&Job::set_progress, job.get(), _1, false));
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
		N_("Wrote %1 FULL, %2 FAKE, %3 REPEAT, %4 pushed to disk"), _full_written, _fake_written, _repeat_written, _pushed_to_disk
		);
}

bool
Writer::check_existing_picture_asset_frame (FILE* asset, int f, Eyes eyes)
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

	/* Read the data from the asset and hash it */
	dcpomatic_fseek (asset, info.offset, SEEK_SET);
	Data data (info.size);
	size_t const read = fread (data.data().get(), 1, data.size(), asset);
	if (read != static_cast<size_t> (data.size ())) {
		LOG_GENERAL ("Existing frame %1 is incomplete", f);
		return false;
	}

	MD5Digester digester;
	digester.add (data.data().get(), data.size());
	if (digester.get() != info.hash) {
		LOG_GENERAL ("Existing frame %1 failed hash check", f);
		return false;
	}

	return true;
}

void
Writer::check_existing_picture_asset ()
{
	/* Try to open the existing asset */
	FILE* asset = fopen_boost (_picture_asset->file(), "rb");
	if (!asset) {
		LOG_GENERAL ("Could not open existing asset at %1 (errno=%2)", _picture_asset->file().string(), errno);
		return;
	}

	while (true) {

		shared_ptr<Job> job = _job.lock ();
		DCPOMATIC_ASSERT (job);

		job->set_progress_unknown ();

		if (_film->three_d ()) {
			if (!check_existing_picture_asset_frame (asset, _first_nonexistant_frame, EYES_LEFT)) {
				break;
			}
			if (!check_existing_picture_asset_frame (asset, _first_nonexistant_frame, EYES_RIGHT)) {
				break;
			}
		} else {
			if (!check_existing_picture_asset_frame (asset, _first_nonexistant_frame, EYES_BOTH)) {
				break;
			}
		}

		LOG_DEBUG_ENCODE ("Have existing frame %1", _first_nonexistant_frame);
		++_first_nonexistant_frame;
	}

	fclose (asset);
}

/** @param frame Frame index.
 *  @return true if we can fake-write this frame.
 */
bool
Writer::can_fake_write (int frame) const
{
	/* We have to do a proper write of the first frame so that we can set up the JPEG2000
	   parameters in the asset writer.
	*/
	return (frame != 0 && frame < _first_nonexistant_frame);
}

void
Writer::write (PlayerSubtitles subs)
{
	if (subs.text.empty ()) {
		return;
	}

	if (!_subtitle_asset) {
		string lang = _film->subtitle_language ();
		if (lang.empty ()) {
			lang = "Unknown";
		}
		if (_film->interop ()) {
			shared_ptr<dcp::InteropSubtitleAsset> s (new dcp::InteropSubtitleAsset ());
			s->set_movie_title (_film->name ());
			s->set_language (lang);
			s->set_reel_number ("1");
			_subtitle_asset = s;
		} else {
			shared_ptr<dcp::SMPTESubtitleAsset> s (new dcp::SMPTESubtitleAsset ());
			s->set_content_title_text (_film->name ());
			s->set_language (lang);
			s->set_edit_rate (dcp::Fraction (_film->video_frame_rate (), 1));
			s->set_time_code_rate (_film->video_frame_rate ());
			_subtitle_asset = s;
		}
	}

	for (list<dcp::SubtitleString>::const_iterator i = subs.text.begin(); i != subs.text.end(); ++i) {
		_subtitle_asset->add (*i);
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

long
Writer::frame_info_position (int frame, Eyes eyes) const
{
	static int const info_size = 48;

	switch (eyes) {
	case EYES_BOTH:
		return frame * info_size;
	case EYES_LEFT:
		return frame * info_size * 2;
	case EYES_RIGHT:
		return frame * info_size * 2 + info_size;
	default:
		DCPOMATIC_ASSERT (false);
	}


	DCPOMATIC_ASSERT (false);
}

dcp::FrameInfo
Writer::read_frame_info (FILE* file, int frame, Eyes eyes) const
{
	dcp::FrameInfo info;
	dcpomatic_fseek (file, frame_info_position (frame, eyes), SEEK_SET);
	fread (&info.offset, sizeof (info.offset), 1, file);
	fread (&info.size, sizeof (info.size), 1, file);

	char hash_buffer[33];
	fread (hash_buffer, 1, 32, file);
	hash_buffer[32] = '\0';
	info.hash = hash_buffer;

	return info;
}
