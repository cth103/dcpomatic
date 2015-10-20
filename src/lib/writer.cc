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
#include "referenced_reel_asset.h"
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
#include <dcp/certificate_chain.h>
#include <dcp/interop_subtitle_asset.h>
#include <dcp/smpte_subtitle_asset.h>
#include <boost/foreach.hpp>
#include <fstream>
#include <cerrno>
#include <iostream>

#include "i18n.h"

#define LOG_GENERAL(...) _film->log()->log (String::compose (__VA_ARGS__), LogEntry::TYPE_GENERAL);
#define LOG_GENERAL_NC(...) _film->log()->log (__VA_ARGS__, LogEntry::TYPE_GENERAL);
#define LOG_DEBUG_ENCODE(...) _film->log()->log (String::compose (__VA_ARGS__), LogEntry::TYPE_DEBUG_ENCODE);
#define LOG_TIMING(...) _film->log()->log (String::compose (__VA_ARGS__), LogEntry::TYPE_TIMING);
#define LOG_WARNING_NC(...) _film->log()->log (__VA_ARGS__, LogEntry::TYPE_WARNING);
#define LOG_WARNING(...) _film->log()->log (String::compose (__VA_ARGS__), LogEntry::TYPE_WARNING);
#define LOG_ERROR(...) _film->log()->log (String::compose (__VA_ARGS__), LogEntry::TYPE_ERROR);

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

int const Writer::_info_size = 48;

Writer::Writer (shared_ptr<const Film> film, weak_ptr<Job> j)
	: _film (film)
	, _job (j)
	, _thread (0)
	, _finish (false)
	, _queued_full_in_memory (0)
	, _last_written_frame (-1)
	, _last_written_eyes (EYES_RIGHT)
	, _maximum_frames_in_memory (0)
	, _full_written (0)
	, _fake_written (0)
	, _repeat_written (0)
	, _ref_written (0)
	, _pushed_to_disk (0)
{
	/* Remove any old DCP */
	boost::filesystem::remove_all (_film->dir (_film->dcp_name ()));

	shared_ptr<Job> job = _job.lock ();
	DCPOMATIC_ASSERT (job);

	BOOST_FOREACH (DCPTimePeriod p, _film->reels ()) {
		Reel reel;
		reel.period = p;

		/* Create our picture asset in a subdirectory, named according to those
		   film's parameters which affect the video output.  We will hard-link
		   it into the DCP later.
		*/

		if (_film->three_d ()) {
			reel.picture_asset.reset (new dcp::StereoPictureAsset (dcp::Fraction (_film->video_frame_rate (), 1)));
		} else {
			reel.picture_asset.reset (new dcp::MonoPictureAsset (dcp::Fraction (_film->video_frame_rate (), 1)));
		}

		reel.picture_asset->set_size (_film->frame_size ());

		if (_film->encrypted ()) {
			reel.picture_asset->set_key (_film->key ());
		}

		reel.picture_asset->set_file (
			_film->internal_video_asset_dir() / _film->internal_video_asset_filename(p)
			);

		job->sub (_("Checking existing image data"));
		check_existing_picture_asset (reel);

		reel.picture_asset_writer = reel.picture_asset->start_write (
			_film->internal_video_asset_dir() / _film->internal_video_asset_filename(p),
			_film->interop() ? dcp::INTEROP : dcp::SMPTE,
			reel.first_nonexistant_frame > 0
			);

		if (_film->audio_channels ()) {
			reel.sound_asset.reset (
				new dcp::SoundAsset (dcp::Fraction (_film->video_frame_rate(), 1), _film->audio_frame_rate (), _film->audio_channels ())
			);

			if (_film->encrypted ()) {
				reel.sound_asset->set_key (_film->key ());
			}

			/* Write the sound asset into the film directory so that we leave the creation
			   of the DCP directory until the last minute.
			*/
			reel.sound_asset_writer = reel.sound_asset->start_write (
				_film->directory() / audio_asset_filename (reel.sound_asset),
				_film->interop() ? dcp::INTEROP : dcp::SMPTE
				);
		}

		_reels.push_back (reel);
	}

	/* We can keep track of the current audio and subtitle reels easily because audio
	   and subs arrive to the Writer in sequence.  This is not so for video.
	*/
	_audio_reel = _reels.begin ();
	_subtitle_reel = _reels.begin ();

	/* Check that the signer is OK if we need one */
	if (_film->is_signed() && !Config::instance()->signer_chain()->valid ()) {
		throw InvalidSignerError ();
	}

	job->sub (_("Encoding image data"));
}

void
Writer::start ()
{
	_thread = new boost::thread (boost::bind (&Writer::thread, this));
}

Writer::~Writer ()
{
	terminate_thread (false);
}

/** @param frame Frame within the DCP */
void
Writer::write (Data encoded, Frame frame, Eyes eyes)
{
	boost::mutex::scoped_lock lock (_state_mutex);

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
Writer::repeat (Frame frame, Eyes eyes)
{
	boost::mutex::scoped_lock lock (_state_mutex);

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
Writer::fake_write (Frame frame, Eyes eyes)
{
	boost::mutex::scoped_lock lock (_state_mutex);

	while (_queued_full_in_memory > _maximum_frames_in_memory) {
		/* The queue is too big; wait until that is sorted out */
		_full_condition.wait (lock);
	}

	Reel const & reel = video_reel (frame);

	FILE* file = fopen_boost (_film->info_file(reel.period), "rb");
	if (!file) {
		throw ReadFileError (_film->info_file(reel.period));
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

void
Writer::ref_write (Frame frame)
{
	boost::mutex::scoped_lock lock (_state_mutex);

	while (_queued_full_in_memory > _maximum_frames_in_memory) {
		/* The queue is too big; wait until that is sorted out */
		_full_condition.wait (lock);
	}

	QueueItem qi;
	qi.type = QueueItem::REF;
	qi.frame = frame;
	qi.eyes = EYES_BOTH;
	_queue.push_back (qi);

	/* Now there's something to do: wake anything wait()ing on _empty_condition */
	_empty_condition.notify_all ();
}

/** Write one video frame's worth of audio frames to the DCP.
 *  This method is not thread safe.
 */
void
Writer::write (shared_ptr<const AudioBuffers> audio)
{
	if (!_audio_reel->sound_asset_writer) {
		return;
	}

	if (audio) {
		_audio_reel->sound_asset_writer->write (audio->data(), audio->frames());
	}

	++_audio_reel->written;

	cout << "(written " << _audio_reel->written << "); period is " << _audio_reel->period.duration() << "\n";

	/* written is in video frames, not audio frames */
	if (_audio_reel->written >= _audio_reel->period.duration().frames_round (_film->video_frame_rate())) {
		cout << "NEXT AUDIO REEL!\n";
		++_audio_reel;
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
	Reel const & reel = video_reel (frame);
	FILE* file = 0;
	boost::filesystem::path info_file = _film->info_file (reel.period);
	if (boost::filesystem::exists (info_file)) {
		file = fopen_boost (info_file, "r+b");
	} else {
		file = fopen_boost (info_file, "wb");
	}
	if (!file) {
		throw OpenFileError (info_file);
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
		boost::mutex::scoped_lock lock (_state_mutex);

		while (true) {

			if (_finish || _queued_full_in_memory > _maximum_frames_in_memory || have_sequenced_image_at_queue_head ()) {
				/* We've got something to do: go and do it */
				break;
			}

			/* Nothing to do: wait until something happens which may indicate that we do */
			LOG_TIMING (N_("writer-sleep queue=%1"), _queue.size());
			_empty_condition.wait (lock);
			LOG_TIMING (N_("writer-wake queue=%1"), _queue.size());
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

			Reel const & reel = video_reel (qi.frame);

			switch (qi.type) {
			case QueueItem::FULL:
			{
				LOG_DEBUG_ENCODE (N_("Writer FULL-writes %1 (%2)"), qi.frame, qi.eyes);
				if (!qi.encoded) {
					qi.encoded = Data (_film->j2c_path (qi.frame, qi.eyes, false));
				}

				dcp::FrameInfo fin = reel.picture_asset_writer->write (qi.encoded->data().get (), qi.encoded->size());
				write_frame_info (qi.frame, qi.eyes, fin);
				_last_written[qi.eyes] = qi.encoded;
				++_full_written;
				break;
			}
			case QueueItem::FAKE:
				LOG_DEBUG_ENCODE (N_("Writer FAKE-writes %1"), qi.frame);
				reel.picture_asset_writer->fake_write (qi.size);
				++_fake_written;
				break;
			case QueueItem::REPEAT:
			{
				LOG_DEBUG_ENCODE (N_("Writer REPEAT-writes %1"), qi.frame);
				dcp::FrameInfo fin = reel.picture_asset_writer->write (
					_last_written[qi.eyes]->data().get(),
					_last_written[qi.eyes]->size()
					);
				write_frame_info (qi.frame, qi.eyes, fin);
				++_repeat_written;
				break;
			}
			case QueueItem::REF:
				LOG_DEBUG_ENCODE (N_("Writer REF-writes %1"), qi.frame);
				++_ref_written;
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
				job->set_progress (float (_full_written + _fake_written + _repeat_written + _ref_written) / total);
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
	boost::mutex::scoped_lock lock (_state_mutex);
	if (_thread == 0) {
		return;
	}

	_finish = true;
	_empty_condition.notify_all ();
	_full_condition.notify_all ();
	lock.unlock ();

	DCPOMATIC_ASSERT (_thread->joinable ());
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

	BOOST_FOREACH (Reel& i, _reels) {

		if (!i.picture_asset_writer->finalize ()) {
			/* Nothing was written to the picture asset */
			i.picture_asset.reset ();
		}

		if (i.sound_asset_writer && !i.sound_asset_writer->finalize ()) {
			/* Nothing was written to the sound asset */
			cout << "nothing written to reel @ " << i.period << "\n";
			i.sound_asset.reset ();
		} else {
			cout << "something written to reel @ " << i.period << "\n";
		}

		/* Hard-link any video asset file into the DCP */
		if (i.picture_asset) {
			boost::filesystem::path video_from = i.picture_asset->file ();
			boost::filesystem::path video_to;
			video_to /= _film->dir (_film->dcp_name());
			video_to /= video_asset_filename (i.picture_asset);

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

			i.picture_asset->set_file (video_to);
		}

		/* Move the audio asset into the DCP */
		if (i.sound_asset) {
			boost::filesystem::path audio_to;
			audio_to /= _film->dir (_film->dcp_name ());
			audio_to /= audio_asset_filename (i.sound_asset);

			boost::system::error_code ec;
			boost::filesystem::rename (_film->file (audio_asset_filename (i.sound_asset)), audio_to, ec);
			if (ec) {
				throw FileError (
					String::compose (_("could not move audio asset into the DCP (%1)"), ec.value ()), audio_asset_filename (i.sound_asset)
					);
			}

			i.sound_asset->set_file (audio_to);
		}
	}

	dcp::DCP dcp (_film->dir (_film->dcp_name()));

	shared_ptr<dcp::CPL> cpl (
		new dcp::CPL (
			_film->dcp_name(),
			_film->dcp_content_type()->libdcp_kind ()
			)
		);

	dcp.add (cpl);

	BOOST_FOREACH (Reel& i, _reels) {
		shared_ptr<dcp::Reel> reel (new dcp::Reel ());

		shared_ptr<dcp::ReelPictureAsset> reel_picture_asset;

		if (i.picture_asset) {
			/* We have made a picture asset of our own.  Put it into the reel */
			shared_ptr<dcp::MonoPictureAsset> mono = dynamic_pointer_cast<dcp::MonoPictureAsset> (i.picture_asset);
			if (mono) {
				reel_picture_asset.reset (new dcp::ReelMonoPictureAsset (mono, 0));
			}

			shared_ptr<dcp::StereoPictureAsset> stereo = dynamic_pointer_cast<dcp::StereoPictureAsset> (i.picture_asset);
			if (stereo) {
				reel_picture_asset.reset (new dcp::ReelStereoPictureAsset (stereo, 0));
			}
		} else {
			/* We don't have a picture asset of our own; hopefully we have one to reference */
			BOOST_FOREACH (ReferencedReelAsset j, _reel_assets) {
				shared_ptr<dcp::ReelPictureAsset> k = dynamic_pointer_cast<dcp::ReelPictureAsset> (j.asset);
				if (k && j.period == i.period) {
					reel_picture_asset = k;
				}
			}
		}

		reel->add (reel_picture_asset);

		if (i.sound_asset) {
			/* We have made a sound asset of our own.  Put it into the reel */
			reel->add (shared_ptr<dcp::ReelSoundAsset> (new dcp::ReelSoundAsset (i.sound_asset, 0)));
		} else {
			/* We don't have a sound asset of our own; hopefully we have one to reference */
			BOOST_FOREACH (ReferencedReelAsset j, _reel_assets) {
				shared_ptr<dcp::ReelSoundAsset> k = dynamic_pointer_cast<dcp::ReelSoundAsset> (j.asset);
				if (k && j.period == i.period) {
					reel->add (k);
				}
			}
		}

		if (i.subtitle_asset) {
			boost::filesystem::path liberation;
			try {
				liberation = shared_path () / "LiberationSans-Regular.ttf";
			} catch (boost::filesystem::filesystem_error& e) {
				/* Hack: try the debian/ubuntu location if getting the shared path failed */
				liberation = "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf";
			}

			/* Add all the fonts to the subtitle content */
			BOOST_FOREACH (shared_ptr<Font> j, _fonts) {
				i.subtitle_asset->add_font (j->id(), j->file().get_value_or (liberation));
			}

			if (dynamic_pointer_cast<dcp::InteropSubtitleAsset> (i.subtitle_asset)) {
				boost::filesystem::path directory = _film->dir (_film->dcp_name ()) / i.subtitle_asset->id ();
				boost::filesystem::create_directories (directory);
				i.subtitle_asset->write (directory / ("sub_" + i.subtitle_asset->id() + ".xml"));
			} else {

				/* All our assets should be the same length; use the picture asset length here
				   as a reference to set the subtitle one.
				*/
				dynamic_pointer_cast<dcp::SMPTESubtitleAsset>(i.subtitle_asset)->set_intrinsic_duration (
					reel_picture_asset->intrinsic_duration ()
					);

				i.subtitle_asset->write (
					_film->dir (_film->dcp_name ()) / ("sub_" + i.subtitle_asset->id() + ".mxf")
					);
			}

			reel->add (shared_ptr<dcp::ReelSubtitleAsset> (
					   new dcp::ReelSubtitleAsset (
						   i.subtitle_asset,
						   dcp::Fraction (_film->video_frame_rate(), 1),
						   reel_picture_asset->intrinsic_duration (),
						   0
						   )
					   ));
		} else {
			/* We don't have a subtitle asset of our own; hopefully we have one to reference */
			BOOST_FOREACH (ReferencedReelAsset j, _reel_assets) {
				shared_ptr<dcp::ReelSubtitleAsset> k = dynamic_pointer_cast<dcp::ReelSubtitleAsset> (j.asset);
				if (k && j.period == i.period) {
					reel->add (k);
				}
			}
		}

		cpl->add (reel);

		shared_ptr<Job> job = _job.lock ();
		DCPOMATIC_ASSERT (job);

		job->sub (_("Computing image digest"));
		if (i.picture_asset) {
			i.picture_asset->hash (boost::bind (&Job::set_progress, job.get(), _1, false));
		}

		if (i.sound_asset) {
			job->sub (_("Computing audio digest"));
			i.sound_asset->hash (boost::bind (&Job::set_progress, job.get(), _1, false));
		}
	}

	dcp::XMLMetadata meta;
	meta.creator = Config::instance()->dcp_creator ();
	if (meta.creator.empty ()) {
		meta.creator = String::compose ("DCP-o-matic %1 %2", dcpomatic_version, dcpomatic_git_commit);
	}
	meta.issuer = Config::instance()->dcp_issuer ();
	if (meta.issuer.empty ()) {
		meta.issuer = String::compose ("DCP-o-matic %1 %2", dcpomatic_version, dcpomatic_git_commit);
	}
	meta.set_issue_date_now ();

	cpl->set_metadata (meta);

	shared_ptr<const dcp::CertificateChain> signer;
	if (_film->is_signed ()) {
		signer = Config::instance()->signer_chain ();
		/* We did check earlier, but check again here to be on the safe side */
		if (!signer->valid ()) {
			throw InvalidSignerError ();
		}
	}

	dcp.write_xml (_film->interop () ? dcp::INTEROP : dcp::SMPTE, meta, signer);

	LOG_GENERAL (
		N_("Wrote %1 FULL, %2 FAKE, %3 REPEAT, %4 REF, %5 pushed to disk"), _full_written, _fake_written, _repeat_written, _ref_written, _pushed_to_disk
		);
}

void
Writer::check_existing_picture_asset (Reel& reel)
{
	/* Try to open the existing asset */
	FILE* asset_file = fopen_boost (reel.picture_asset->file(), "rb");
	if (!asset_file) {
		LOG_GENERAL ("Could not open existing asset at %1 (errno=%2)", reel.picture_asset->file().string(), errno);
		return;
	}

	/* Offset of the last dcp::FrameInfo in the info file */
	int const n = (boost::filesystem::file_size (_film->info_file(reel.period)) / _info_size) - 1;

	FILE* info_file = fopen_boost (_film->info_file(reel.period), "rb");
	if (!info_file) {
		LOG_GENERAL_NC ("Could not open film info file");
		fclose (asset_file);
		return;
	}

	if (_film->three_d ()) {
		/* Start looking at the last left frame */
		reel.first_nonexistant_frame = n / 2;
	} else {
		reel.first_nonexistant_frame = n;
	}

	bool ok = false;

	while (!ok) {
		/* Read the data from the info file; for 3D we just check the left
		   frames until we find a good one.
		*/
		dcp::FrameInfo info = read_frame_info (info_file, reel.first_nonexistant_frame, _film->three_d () ? EYES_LEFT : EYES_BOTH);

		ok = true;

		/* Read the data from the asset and hash it */
		dcpomatic_fseek (asset_file, info.offset, SEEK_SET);
		Data data (info.size);
		size_t const read = fread (data.data().get(), 1, data.size(), asset_file);
		if (read != static_cast<size_t> (data.size ())) {
			LOG_GENERAL ("Existing frame %1 is incomplete", reel.first_nonexistant_frame);
			ok = false;
		} else {
			MD5Digester digester;
			digester.add (data.data().get(), data.size());
			if (digester.get() != info.hash) {
				LOG_GENERAL ("Existing frame %1 failed hash check", reel.first_nonexistant_frame);
				ok = false;
			}
		}

		if (!ok) {
			--reel.first_nonexistant_frame;
		}
	}

	if (!_film->three_d ()) {
		/* If we are doing 3D we might have found a good L frame with no R, so only
		   do this if we're in 2D and we've just found a good B(oth) frame.
		*/
		++reel.first_nonexistant_frame;
	}

	fclose (asset_file);
	fclose (info_file);
}

/** @param frame Frame index within the whole DCP.
 *  @return true if we can fake-write this frame.
 */
bool
Writer::can_fake_write (Frame frame) const
{
	/* We have to do a proper write of the first frame so that we can set up the JPEG2000
	   parameters in the asset writer.
	*/

	Reel const & reel = video_reel (frame);

	/* Make frame relative to the start of the reel */
	frame -= reel.period.from.frames_floor (_film->video_frame_rate());
	return (frame != 0 && frame < reel.first_nonexistant_frame);
}

void
Writer::write (PlayerSubtitles subs)
{
	if (subs.text.empty ()) {
		return;
	}

	if (_subtitle_reel->period.to < subs.from) {
		++_subtitle_reel;
	}

	if (!_subtitle_reel->subtitle_asset) {
		string lang = _film->subtitle_language ();
		if (lang.empty ()) {
			lang = "Unknown";
		}
		if (_film->interop ()) {
			shared_ptr<dcp::InteropSubtitleAsset> s (new dcp::InteropSubtitleAsset ());
			s->set_movie_title (_film->name ());
			s->set_language (lang);
			s->set_reel_number ("1");
			_subtitle_reel->subtitle_asset = s;
		} else {
			shared_ptr<dcp::SMPTESubtitleAsset> s (new dcp::SMPTESubtitleAsset ());
			s->set_content_title_text (_film->name ());
			s->set_language (lang);
			s->set_edit_rate (dcp::Fraction (_film->video_frame_rate (), 1));
			s->set_reel_number (1);
			s->set_time_code_rate (_film->video_frame_rate ());
			s->set_start_time (dcp::Time ());
			_subtitle_reel->subtitle_asset = s;
		}
	}

	for (list<dcp::SubtitleString>::const_iterator i = subs.text.begin(); i != subs.text.end(); ++i) {
		_subtitle_reel->subtitle_asset->add (*i);
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
	_maximum_frames_in_memory = lrint (threads * 1.1);
}

long
Writer::frame_info_position (int frame, Eyes eyes) const
{
	switch (eyes) {
	case EYES_BOTH:
		return frame * _info_size;
	case EYES_LEFT:
		return frame * _info_size * 2;
	case EYES_RIGHT:
		return frame * _info_size * 2 + _info_size;
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

void
Writer::write (ReferencedReelAsset asset)
{
	_reel_assets.push_back (asset);
}

Writer::Reel const &
Writer::video_reel (int frame) const
{
	DCPTime t = DCPTime::from_frames (frame, _film->video_frame_rate ());
	list<Reel>::const_iterator i = _reels.begin ();
	while (i != _reels.end() && !i->period.contains (t)) {
		++i;
	}

	DCPOMATIC_ASSERT (i != _reels.end ());
	return *i;
}
