/*
    Copyright (C) 2012-2017 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

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
#include "version.h"
#include "font.h"
#include "util.h"
#include "reel_writer.h"
#include <dcp/cpl.h>
#include <dcp/locale_convert.h>
#include <boost/foreach.hpp>
#include <fstream>
#include <cerrno>
#include <iostream>
#include <cfloat>

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
using std::map;
using std::min;
using std::max;
using boost::shared_ptr;
using boost::weak_ptr;
using boost::dynamic_pointer_cast;
using dcp::Data;

Writer::Writer (shared_ptr<const Film> film, weak_ptr<Job> j)
	: _film (film)
	, _job (j)
	, _thread (0)
	, _finish (false)
	, _queued_full_in_memory (0)
	/* These will be reset to sensible values when J2KEncoder is created */
	, _maximum_frames_in_memory (8)
	, _maximum_queue_size (8)
	, _full_written (0)
	, _fake_written (0)
	, _repeat_written (0)
	, _pushed_to_disk (0)
{
	shared_ptr<Job> job = _job.lock ();
	DCPOMATIC_ASSERT (job);

	int reel_index = 0;
	list<DCPTimePeriod> const reels = _film->reels ();
	BOOST_FOREACH (DCPTimePeriod p, reels) {
		_reels.push_back (ReelWriter (film, p, job, reel_index++, reels.size(), _film->content_summary(p)));
	}

	/* We can keep track of the current audio, subtitle and closed caption reels easily because audio
	   and captions arrive to the Writer in sequence.  This is not so for video.
	*/
	_audio_reel = _reels.begin ();
	for (int i = 0; i < CAPTION_COUNT; ++i) {
		_caption_reel[i] = _reels.begin ();
	}

	/* Check that the signer is OK if we need one */
	string reason;
	if (_film->is_signed() && !Config::instance()->signer_chain()->valid(&reason)) {
		throw InvalidSignerError (reason);
	}
}

void
Writer::start ()
{
	_thread = new boost::thread (boost::bind (&Writer::thread, this));
#ifdef DCPOMATIC_LINUX
	pthread_setname_np (_thread->native_handle(), "writer");
#endif
}

Writer::~Writer ()
{
	terminate_thread (false);
}

/** Pass a video frame to the writer for writing to disk at some point.
 *  This method can be called with frames out of order.
 *  @param encoded JPEG2000-encoded data.
 *  @param frame Frame index within the DCP.
 *  @param eyes Eyes that this frame image is for.
 */
void
Writer::write (Data encoded, Frame frame, Eyes eyes)
{
	boost::mutex::scoped_lock lock (_state_mutex);

	while (_queued_full_in_memory > _maximum_frames_in_memory) {
		/* There are too many full frames in memory; wake the main writer thread and
		   wait until it sorts everything out */
		_empty_condition.notify_all ();
		_full_condition.wait (lock);
	}

	QueueItem qi;
	qi.type = QueueItem::FULL;
	qi.encoded = encoded;
	qi.reel = video_reel (frame);
	qi.frame = frame - _reels[qi.reel].start ();

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

bool
Writer::can_repeat (Frame frame) const
{
	return frame > _reels[video_reel(frame)].start();
}

/** Repeat the last frame that was written to a reel as a new frame.
 *  @param frame Frame index within the DCP of the new (repeated) frame.
 *  @param eyes Eyes that this repeated frame image is for.
 */
void
Writer::repeat (Frame frame, Eyes eyes)
{
	boost::mutex::scoped_lock lock (_state_mutex);

	while (_queue.size() > _maximum_queue_size && have_sequenced_image_at_queue_head()) {
		/* The queue is too big, and the main writer thread can run and fix it, so
		   wake it and wait until it has done.
		*/
		_empty_condition.notify_all ();
		_full_condition.wait (lock);
	}

	QueueItem qi;
	qi.type = QueueItem::REPEAT;
	qi.reel = video_reel (frame);
	qi.frame = frame - _reels[qi.reel].start ();
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

	while (_queue.size() > _maximum_queue_size && have_sequenced_image_at_queue_head()) {
		/* The queue is too big, and the main writer thread can run and fix it, so
		   wake it and wait until it has done.
		*/
		_empty_condition.notify_all ();
		_full_condition.wait (lock);
	}

	size_t const reel = video_reel (frame);
	Frame const reel_frame = frame - _reels[reel].start ();

	FILE* file = fopen_boost (_film->info_file(_reels[reel].period()), "rb");
	if (!file) {
		throw ReadFileError (_film->info_file(_reels[reel].period()));
	}
	dcp::FrameInfo info = _reels[reel].read_frame_info (file, reel_frame, eyes);
	fclose (file);

	QueueItem qi;
	qi.type = QueueItem::FAKE;
	qi.size = info.size;
	qi.reel = reel;
	qi.frame = reel_frame;
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

/** Write some audio frames to the DCP.
 *  @param audio Audio data.
 *  @param time Time of this data within the DCP.
 *  This method is not thread safe.
 */
void
Writer::write (shared_ptr<const AudioBuffers> audio, DCPTime const time)
{
	DCPOMATIC_ASSERT (audio);

	int const afr = _film->audio_frame_rate();

	DCPTime const end = time + DCPTime::from_frames(audio->frames(), afr);

	/* The audio we get might span a reel boundary, and if so we have to write it in bits */

	DCPTime t = time;
	while (t < end) {

		if (_audio_reel == _reels.end ()) {
			/* This audio is off the end of the last reel; ignore it */
			return;
		}

		if (end <= _audio_reel->period().to) {
			/* Easy case: we can write all the audio to this reel */
			_audio_reel->write (audio);
			t = end;
		} else {
			/* Split the audio into two and write the first part */
			DCPTime part_lengths[2] = {
				_audio_reel->period().to - t,
				end - _audio_reel->period().to
			};

			Frame part_frames[2] = {
				part_lengths[0].frames_ceil(afr),
				part_lengths[1].frames_ceil(afr)
			};

			if (part_frames[0]) {
				shared_ptr<AudioBuffers> part (new AudioBuffers (audio->channels(), part_frames[0]));
				part->copy_from (audio.get(), part_frames[0], 0, 0);
				_audio_reel->write (part);
			}

			if (part_frames[1]) {
				shared_ptr<AudioBuffers> part (new AudioBuffers (audio->channels(), part_frames[1]));
				part->copy_from (audio.get(), part_frames[1], part_frames[0], 0);
				audio = part;
			} else {
				audio.reset ();
			}

			++_audio_reel;
			t += part_lengths[0];
		}
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

	QueueItem const & f = _queue.front();
	ReelWriter const & reel = _reels[f.reel];

	/* The queue should contain only EYES_LEFT/EYES_RIGHT pairs or EYES_BOTH */

	if (f.eyes == EYES_BOTH) {
		/* 2D */
		return f.frame == (reel.last_written_video_frame() + 1);
	}

	/* 3D */

	if (reel.last_written_eyes() == EYES_LEFT && f.frame == reel.last_written_video_frame() && f.eyes == EYES_RIGHT) {
		return true;
	}

	if (reel.last_written_eyes() == EYES_RIGHT && f.frame == (reel.last_written_video_frame() + 1) && f.eyes == EYES_LEFT) {
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
						LOG_WARNING (N_("- type FULL, frame %1, eyes %2"), i->frame, (int) i->eyes);
					} else {
						LOG_WARNING (N_("- type FAKE, size %1, frame %2, eyes %3"), i->size, i->frame, (int) i->eyes);
					}
				}
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

			ReelWriter& reel = _reels[qi.reel];

			switch (qi.type) {
			case QueueItem::FULL:
				LOG_DEBUG_ENCODE (N_("Writer FULL-writes %1 (%2)"), qi.frame, (int) qi.eyes);
				if (!qi.encoded) {
					qi.encoded = Data (_film->j2c_path (qi.reel, qi.frame, qi.eyes, false));
				}
				reel.write (qi.encoded, qi.frame, qi.eyes);
				++_full_written;
				break;
			case QueueItem::FAKE:
				LOG_DEBUG_ENCODE (N_("Writer FAKE-writes %1"), qi.frame);
				reel.fake_write (qi.frame, qi.eyes, qi.size);
				++_fake_written;
				break;
			case QueueItem::REPEAT:
				LOG_DEBUG_ENCODE (N_("Writer REPEAT-writes %1"), qi.frame);
				reel.repeat_write (qi.frame, qi.eyes);
				++_repeat_written;
				break;
			}

			lock.lock ();
			_full_condition.notify_all ();
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
			/* For the log message below */
			int const awaiting = _reels[_queue.front().reel].last_written_video_frame();
			lock.unlock ();

			/* i is valid here, even though we don't hold a lock on the mutex,
			   since list iterators are unaffected by insertion and only this
			   thread could erase the last item in the list.
			*/

			LOG_GENERAL ("Writer full; pushes %1 to disk while awaiting %2", i->frame, awaiting);

			i->encoded->write_via_temp (
				_film->j2c_path (i->reel, i->frame, i->eyes, true),
				_film->j2c_path (i->reel, i->frame, i->eyes, false)
				);

			lock.lock ();
			i->encoded.reset ();
			--_queued_full_in_memory;
			_full_condition.notify_all ();
		}
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

	if (_thread->joinable ()) {
		_thread->join ();
	}

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

	LOG_GENERAL_NC ("Terminating writer thread");

	terminate_thread (true);

	LOG_GENERAL_NC ("Finishing ReelWriters");

	BOOST_FOREACH (ReelWriter& i, _reels) {
		i.finish ();
	}

	LOG_GENERAL_NC ("Writing XML");

	dcp::DCP dcp (_film->dir (_film->dcp_name()));

	shared_ptr<dcp::CPL> cpl (
		new dcp::CPL (
			_film->dcp_name(),
			_film->dcp_content_type()->libdcp_kind ()
			)
		);

	dcp.add (cpl);

	/* Calculate digests for each reel in parallel */

	shared_ptr<Job> job = _job.lock ();
	job->sub (_("Computing digests"));

	boost::asio::io_service service;
	boost::thread_group pool;

	shared_ptr<boost::asio::io_service::work> work (new boost::asio::io_service::work (service));

	int const threads = max (1, Config::instance()->master_encoding_threads ());

	for (int i = 0; i < threads; ++i) {
		pool.create_thread (boost::bind (&boost::asio::io_service::run, &service));
	}

	BOOST_FOREACH (ReelWriter& i, _reels) {
		boost::function<void (float)> set_progress = boost::bind (&Writer::set_digest_progress, this, job.get(), _1);
		service.post (boost::bind (&ReelWriter::calculate_digests, &i, set_progress));
	}

	work.reset ();
	pool.join_all ();
	service.stop ();

	/* Add reels to CPL */

	BOOST_FOREACH (ReelWriter& i, _reels) {
		cpl->add (i.create_reel (_reel_assets, _fonts));
	}

	dcp::XMLMetadata meta;
	meta.annotation_text = cpl->annotation_text ();
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
		string reason;
		if (!signer->valid (&reason)) {
			throw InvalidSignerError (reason);
		}
	}

	dcp.write_xml (_film->interop () ? dcp::INTEROP : dcp::SMPTE, meta, signer, Config::instance()->dcp_metadata_filename_format());

	LOG_GENERAL (
		N_("Wrote %1 FULL, %2 FAKE, %3 REPEAT, %4 pushed to disk"), _full_written, _fake_written, _repeat_written, _pushed_to_disk
		);

	write_cover_sheet ();
}

void
Writer::write_cover_sheet ()
{
	boost::filesystem::path const cover = _film->file ("COVER_SHEET.txt");
	FILE* f = fopen_boost (cover, "w");
	if (!f) {
		throw OpenFileError (cover, errno, false);
	}

	string text = Config::instance()->cover_sheet ();
	boost::algorithm::replace_all (text, "$CPL_NAME", _film->name());
	boost::algorithm::replace_all (text, "$TYPE", _film->dcp_content_type()->pretty_name());
	boost::algorithm::replace_all (text, "$CONTAINER", _film->container()->container_nickname());
	boost::algorithm::replace_all (text, "$AUDIO_LANGUAGE", _film->isdcf_metadata().audio_language);
	boost::algorithm::replace_all (text, "$SUBTITLE_LANGUAGE", _film->isdcf_metadata().subtitle_language);

	boost::uintmax_t size = 0;
	for (
		boost::filesystem::recursive_directory_iterator i = boost::filesystem::recursive_directory_iterator(_film->dir(_film->dcp_name()));
		i != boost::filesystem::recursive_directory_iterator();
		++i) {
		if (boost::filesystem::is_regular_file (i->path ())) {
			size += boost::filesystem::file_size (i->path ());
		}
	}

	if (size > (1000000000L)) {
		boost::algorithm::replace_all (text, "$SIZE", String::compose ("%1GB", dcp::locale_convert<string> (size / 1000000000.0, 1, true)));
	} else {
		boost::algorithm::replace_all (text, "$SIZE", String::compose ("%1MB", dcp::locale_convert<string> (size / 1000000.0, 1, true)));
	}

	pair<int, int> ch = audio_channel_types (_film->mapped_audio_channels(), _film->audio_channels());
	string description = String::compose("%1.%2", ch.first, ch.second);

	if (description == "0.0") {
		description = _("None");
	} else if (description == "1.0") {
		description = _("Mono");
	} else if (description == "2.0") {
		description = _("Stereo");
	}
	boost::algorithm::replace_all (text, "$AUDIO", description);

	int h, m, s, fr;
	_film->length().split (_film->video_frame_rate(), h, m, s, fr);
	string length;
	if (h == 0 && m == 0) {
		length = String::compose("%1s", s);
	} else if (h == 0 && m > 0) {
		length = String::compose("%1m%2s", m, s);
	} else if (h > 0 && m > 0) {
		length = String::compose("%1h%2m%3s", h, m, s);
	}

	boost::algorithm::replace_all (text, "$LENGTH", length);

	fwrite (text.c_str(), 1, text.length(), f);
	fclose (f);
}

/** @param frame Frame index within the whole DCP.
 *  @return true if we can fake-write this frame.
 */
bool
Writer::can_fake_write (Frame frame) const
{
	if (_film->encrypted()) {
		/* We need to re-write the frame because the asset ID is embedded in the HMAC... I think... */
		return false;
	}

	/* We have to do a proper write of the first frame so that we can set up the JPEG2000
	   parameters in the asset writer.
	*/

	ReelWriter const & reel = _reels[video_reel(frame)];

	/* Make frame relative to the start of the reel */
	frame -= reel.start ();
	return (frame != 0 && frame < reel.first_nonexistant_frame());
}

void
Writer::write (PlayerText text, TextType type, DCPTimePeriod period)
{
	while (_caption_reel[type]->period().to <= period.from) {
		++_caption_reel[type];
		DCPOMATIC_ASSERT (_caption_reel[type] != _reels.end());
	}

	DCPOMATIC_ASSERT (_caption_reel[type] != _reels.end());

	_caption_reel[type]->write (text, type, period);
}

void
Writer::write (list<shared_ptr<Font> > fonts)
{
	/* Just keep a list of unique fonts and we'll deal with them in ::finish */

	BOOST_FOREACH (shared_ptr<Font> i, fonts) {
		bool got = false;
		BOOST_FOREACH (shared_ptr<Font> j, _fonts) {
			if (*i == *j) {
				got = true;
			}
		}

		if (!got) {
			_fonts.push_back (i);
		}
	}
}

bool
operator< (QueueItem const & a, QueueItem const & b)
{
	if (a.reel != b.reel) {
		return a.reel < b.reel;
	}

	if (a.frame != b.frame) {
		return a.frame < b.frame;
	}

	return static_cast<int> (a.eyes) < static_cast<int> (b.eyes);
}

bool
operator== (QueueItem const & a, QueueItem const & b)
{
	return a.reel == b.reel && a.frame == b.frame && a.eyes == b.eyes;
}

void
Writer::set_encoder_threads (int threads)
{
	boost::mutex::scoped_lock lm (_state_mutex);
	_maximum_frames_in_memory = lrint (threads * Config::instance()->frames_in_memory_multiplier());
	_maximum_queue_size = threads * 16;
}

void
Writer::write (ReferencedReelAsset asset)
{
	_reel_assets.push_back (asset);
}

size_t
Writer::video_reel (int frame) const
{
	DCPTime t = DCPTime::from_frames (frame, _film->video_frame_rate ());
	size_t i = 0;
	while (i < _reels.size() && !_reels[i].period().contains (t)) {
		++i;
	}

	DCPOMATIC_ASSERT (i < _reels.size ());
	return i;
}

void
Writer::set_digest_progress (Job* job, float progress)
{
	/* I believe this is thread-safe */
	_digest_progresses[boost::this_thread::get_id()] = progress;

	boost::mutex::scoped_lock lm (_digest_progresses_mutex);
	float min_progress = FLT_MAX;
	for (map<boost::thread::id, float>::const_iterator i = _digest_progresses.begin(); i != _digest_progresses.end(); ++i) {
		min_progress = min (min_progress, i->second);
	}

	job->set_progress (min_progress);
}
