/*
    Copyright (C) 2012-2019 Carl Hetherington <cth@carlh.net>

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
#include "dcpomatic_log.h"
#include "dcp_video.h"
#include "dcp_content_type.h"
#include "audio_mapping.h"
#include "config.h"
#include "job.h"
#include "cross.h"
#include "audio_buffers.h"
#include "version.h"
#include "font_data.h"
#include "util.h"
#include "reel_writer.h"
#include "text_content.h"
#include <dcp/cpl.h>
#include <dcp/locale_convert.h>
#include <dcp/reel_mxf.h>
#include <boost/foreach.hpp>
#include <fstream>
#include <cerrno>
#include <iostream>
#include <cfloat>

#include "i18n.h"

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
using std::vector;
using std::shared_ptr;
using std::weak_ptr;
using std::dynamic_pointer_cast;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using dcp::Data;
using dcp::ArrayData;
using namespace dcpomatic;


static
void
ignore_progress (float)
{

}


/** @param j Job to report progress to, or 0.
 *  @param text_only true to enable only the text (subtitle/ccap) parts of the writer.
 */
Writer::Writer (weak_ptr<const Film> weak_film, weak_ptr<Job> j, bool text_only)
	: WeakConstFilm (weak_film)
	, _job (j)
	, _finish (false)
	, _queued_full_in_memory (0)
	/* These will be reset to sensible values when J2KEncoder is created */
	, _maximum_frames_in_memory (8)
	, _maximum_queue_size (8)
	, _full_written (0)
	, _fake_written (0)
	, _repeat_written (0)
	, _pushed_to_disk (0)
	, _text_only (text_only)
	, _have_subtitles (false)
{
	shared_ptr<Job> job = _job.lock ();

	int reel_index = 0;
	list<DCPTimePeriod> const reels = film()->reels();
	BOOST_FOREACH (DCPTimePeriod p, reels) {
		_reels.push_back (ReelWriter(weak_film, p, job, reel_index++, reels.size(), text_only));
	}

	_last_written.resize (reels.size());

	/* We can keep track of the current audio, subtitle and closed caption reels easily because audio
	   and captions arrive to the Writer in sequence.  This is not so for video.
	*/
	_audio_reel = _reels.begin ();
	_subtitle_reel = _reels.begin ();
	BOOST_FOREACH (DCPTextTrack i, film()->closed_caption_tracks()) {
		_caption_reels[i] = _reels.begin ();
	}
	_atmos_reel = _reels.begin ();

	/* Check that the signer is OK */
	string reason;
	if (!Config::instance()->signer_chain()->valid(&reason)) {
		throw InvalidSignerError (reason);
	}
}

void
Writer::start ()
{
	if (!_text_only) {
		_thread = boost::thread (boost::bind(&Writer::thread, this));
#ifdef DCPOMATIC_LINUX
		pthread_setname_np (_thread.native_handle(), "writer");
#endif
	}
}

Writer::~Writer ()
{
	if (!_text_only) {
		terminate_thread (false);
	}
}

/** Pass a video frame to the writer for writing to disk at some point.
 *  This method can be called with frames out of order.
 *  @param encoded JPEG2000-encoded data.
 *  @param frame Frame index within the DCP.
 *  @param eyes Eyes that this frame image is for.
 */
void
Writer::write (shared_ptr<const Data> encoded, Frame frame, Eyes eyes)
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

	if (film()->three_d() && eyes == EYES_BOTH) {
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
	if (film()->three_d() && eyes == EYES_BOTH) {
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
	Frame const frame_in_reel = frame - _reels[reel].start ();

	QueueItem qi;
	qi.type = QueueItem::FAKE;

	{
		shared_ptr<InfoFileHandle> info_file = film()->info_file_handle(_reels[reel].period(), true);
		qi.size = _reels[reel].read_frame_info(info_file, frame_in_reel, eyes).size;
	}

	qi.reel = reel;
	qi.frame = frame_in_reel;
	if (film()->three_d() && eyes == EYES_BOTH) {
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

	int const afr = film()->audio_frame_rate();

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
		} else if (_audio_reel->period().to <= t) {
			/* This reel is entirely before the start of our audio; just skip the reel */
			++_audio_reel;
		} else {
			/* This audio is over a reel boundary; split the audio into two and write the first part */
			DCPTime part_lengths[2] = {
				_audio_reel->period().to - t,
				end - _audio_reel->period().to
			};

			Frame part_frames[2] = {
				part_lengths[0].frames_ceil(afr),
				part_lengths[1].frames_ceil(afr)
			};

			if (part_frames[0]) {
				shared_ptr<AudioBuffers> part (new AudioBuffers(audio, part_frames[0], 0));
				_audio_reel->write (part);
			}

			if (part_frames[1]) {
				audio.reset (new AudioBuffers(audio, part_frames[1], part_frames[0]));
			} else {
				audio.reset ();
			}

			++_audio_reel;
			t += part_lengths[0];
		}
	}
}


void
Writer::write (shared_ptr<const dcp::AtmosFrame> atmos, DCPTime time, AtmosMetadata metadata)
{
	if (_atmos_reel->period().to == time) {
		++_atmos_reel;
		DCPOMATIC_ASSERT (_atmos_reel != _reels.end());
	}

	/* We assume that we get a video frame's worth of data here */
	_atmos_reel->write (atmos, metadata);
}


/** Caller must hold a lock on _state_mutex */
bool
Writer::have_sequenced_image_at_queue_head ()
{
	if (_queue.empty ()) {
		return false;
	}

	_queue.sort ();
	QueueItem const & f = _queue.front();
	return _last_written[f.reel].next(f);
}


bool
Writer::LastWritten::next (QueueItem qi) const
{
	if (qi.eyes == EYES_BOTH) {
		/* 2D */
		return qi.frame == (_frame + 1);
	}

	/* 3D */

	if (_eyes == EYES_LEFT && qi.frame == _frame && qi.eyes == EYES_RIGHT) {
		return true;
	}

	if (_eyes == EYES_RIGHT && qi.frame == (_frame + 1) && qi.eyes == EYES_LEFT) {
		return true;
	}

	return false;
}


void
Writer::LastWritten::update (QueueItem qi)
{
	_frame = qi.frame;
	_eyes = qi.eyes;
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

		/* We stop here if we have been asked to finish, and if either the queue
		   is empty or we do not have a sequenced image at its head (if this is the
		   case we will never terminate as no new frames will be sent once
		   _finish is true).
		*/
		if (_finish && (!have_sequenced_image_at_queue_head() || _queue.empty())) {
			/* (Hopefully temporarily) log anything that was not written */
			if (!_queue.empty() && !have_sequenced_image_at_queue_head()) {
				LOG_WARNING (N_("Finishing writer with a left-over queue of %1:"), _queue.size());
				BOOST_FOREACH (QueueItem const& i, _queue) {
					if (i.type == QueueItem::FULL) {
						LOG_WARNING (N_("- type FULL, frame %1, eyes %2"), i.frame, (int) i.eyes);
					} else {
						LOG_WARNING (N_("- type FAKE, size %1, frame %2, eyes %3"), i.size, i.frame, (int) i.eyes);
					}
				}
			}
			return;
		}

		/* Write any frames that we can write; i.e. those that are in sequence. */
		while (have_sequenced_image_at_queue_head ()) {
			QueueItem qi = _queue.front ();
			_last_written[qi.reel].update (qi);
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
					qi.encoded.reset (new ArrayData(film()->j2c_path(qi.reel, qi.frame, qi.eyes, false)));
				}
				reel.write (qi.encoded, qi.frame, qi.eyes);
				++_full_written;
				break;
			case QueueItem::FAKE:
				LOG_DEBUG_ENCODE (N_("Writer FAKE-writes %1"), qi.frame);
				reel.fake_write (qi.size);
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
			int const awaiting = _last_written[_queue.front().reel].frame() + 1;
			lock.unlock ();

			/* i is valid here, even though we don't hold a lock on the mutex,
			   since list iterators are unaffected by insertion and only this
			   thread could erase the last item in the list.
			*/

			LOG_GENERAL ("Writer full; pushes %1 to disk while awaiting %2", i->frame, awaiting);

			i->encoded->write_via_temp (
				film()->j2c_path(i->reel, i->frame, i->eyes, true),
				film()->j2c_path(i->reel, i->frame, i->eyes, false)
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
	boost::this_thread::disable_interruption dis;

	boost::mutex::scoped_lock lock (_state_mutex);

	_finish = true;
	_empty_condition.notify_all ();
	_full_condition.notify_all ();
	lock.unlock ();

	try {
		_thread.join ();
	} catch (...) {}

	if (can_throw) {
		rethrow ();
	}
}


/** @param output_dcp Path to DCP folder to write */
void
Writer::finish (boost::filesystem::path output_dcp)
{
	if (_thread.joinable()) {
		LOG_GENERAL_NC ("Terminating writer thread");
		terminate_thread (true);
	}

	LOG_GENERAL_NC ("Finishing ReelWriters");

	BOOST_FOREACH (ReelWriter& i, _reels) {
		i.finish (output_dcp);
	}

	LOG_GENERAL_NC ("Writing XML");

	dcp::DCP dcp (output_dcp);

	shared_ptr<dcp::CPL> cpl (
		new dcp::CPL (
			film()->dcp_name(),
			film()->dcp_content_type()->libdcp_kind ()
			)
		);

	dcp.add (cpl);

	/* Calculate digests for each reel in parallel */

	shared_ptr<Job> job = _job.lock ();
	if (job) {
		job->sub (_("Computing digests"));
	}

	boost::asio::io_service service;
	boost::thread_group pool;

	shared_ptr<boost::asio::io_service::work> work (new boost::asio::io_service::work (service));

	int const threads = max (1, Config::instance()->master_encoding_threads ());

	for (int i = 0; i < threads; ++i) {
		pool.create_thread (boost::bind (&boost::asio::io_service::run, &service));
	}

	boost::function<void (float)> set_progress;
	if (job) {
		set_progress = boost::bind (&Writer::set_digest_progress, this, job.get(), _1);
	} else {
		set_progress = &ignore_progress;
	}

	BOOST_FOREACH (ReelWriter& i, _reels) {
		service.post (boost::bind (&ReelWriter::calculate_digests, &i, set_progress));
	}
	service.post (boost::bind (&Writer::calculate_referenced_digests, this, set_progress));

	work.reset ();
	pool.join_all ();
	service.stop ();

	/* Add reels */

	BOOST_FOREACH (ReelWriter& i, _reels) {
		cpl->add (i.create_reel(_reel_assets, _fonts, output_dcp, _have_subtitles, _have_closed_captions));
	}

	/* Add metadata */

	string creator = Config::instance()->dcp_creator();
	if (creator.empty()) {
		creator = String::compose("DCP-o-matic %1 %2", dcpomatic_version, dcpomatic_git_commit);
	}

	string issuer = Config::instance()->dcp_issuer();
	if (issuer.empty()) {
		issuer = String::compose("DCP-o-matic %1 %2", dcpomatic_version, dcpomatic_git_commit);
	}

	cpl->set_ratings (film()->ratings());

	vector<dcp::ContentVersion> cv;
	BOOST_FOREACH (string i, film()->content_versions()) {
		cv.push_back (dcp::ContentVersion(i));
	}
	cpl->set_content_versions (cv);

	cpl->set_full_content_title_text (film()->name());
	cpl->set_full_content_title_text_language (film()->name_language());
	cpl->set_release_territory (film()->release_territory());
	cpl->set_version_number (film()->version_number());
	cpl->set_status (film()->status());
	cpl->set_chain (film()->chain());
	cpl->set_distributor (film()->distributor());
	cpl->set_facility (film()->facility());
	cpl->set_luminance (film()->luminance());

	list<int> ac = film()->mapped_audio_channels();
	dcp::MCASoundField field = (
		find(ac.begin(), ac.end(), static_cast<int>(dcp::BSL)) != ac.end() ||
		find(ac.begin(), ac.end(), static_cast<int>(dcp::BSR)) != ac.end()
		) ? dcp::SEVEN_POINT_ONE : dcp::FIVE_POINT_ONE;

	dcp::MainSoundConfiguration msc (field, film()->audio_channels());
	BOOST_FOREACH (int i, ac) {
		if (i < film()->audio_channels()) {
			msc.set_mapping (i, static_cast<dcp::Channel>(i));
		}
	}

	cpl->set_main_sound_configuration (msc.to_string());
	cpl->set_main_sound_sample_rate (film()->audio_frame_rate());
	cpl->set_main_picture_stored_area (film()->frame_size());
	cpl->set_main_picture_active_area (film()->active_area());

	vector<dcp::LanguageTag> sl = film()->subtitle_languages();
	if (sl.size() > 1) {
		cpl->set_additional_subtitle_languages(std::vector<dcp::LanguageTag>(sl.begin() + 1, sl.end()));
	}

	shared_ptr<const dcp::CertificateChain> signer;
	signer = Config::instance()->signer_chain ();
	/* We did check earlier, but check again here to be on the safe side */
	string reason;
	if (!signer->valid (&reason)) {
		throw InvalidSignerError (reason);
	}

	dcp.write_xml (
		film()->interop() ? dcp::INTEROP : dcp::SMPTE,
		issuer,
		creator,
		dcp::LocalTime().as_string(),
		String::compose("Created by libdcp %1", dcp::version),
		signer,
		Config::instance()->dcp_metadata_filename_format()
		);

	LOG_GENERAL (
		N_("Wrote %1 FULL, %2 FAKE, %3 REPEAT, %4 pushed to disk"), _full_written, _fake_written, _repeat_written, _pushed_to_disk
		);

	write_cover_sheet (output_dcp);
}

void
Writer::write_cover_sheet (boost::filesystem::path output_dcp)
{
	boost::filesystem::path const cover = film()->file("COVER_SHEET.txt");
	FILE* f = fopen_boost (cover, "w");
	if (!f) {
		throw OpenFileError (cover, errno, OpenFileError::WRITE);
	}

	string text = Config::instance()->cover_sheet ();
	boost::algorithm::replace_all (text, "$CPL_NAME", film()->name());
	boost::algorithm::replace_all (text, "$TYPE", film()->dcp_content_type()->pretty_name());
	boost::algorithm::replace_all (text, "$CONTAINER", film()->container()->container_nickname());
	boost::algorithm::replace_all (text, "$AUDIO_LANGUAGE", film()->isdcf_metadata().audio_language);

	vector<dcp::LanguageTag> subtitle_languages = film()->subtitle_languages();
	if (subtitle_languages.empty()) {
		boost::algorithm::replace_all (text, "$SUBTITLE_LANGUAGE", "None");
	} else {
		boost::algorithm::replace_all (text, "$SUBTITLE_LANGUAGE", subtitle_languages.front().description());
	}

	boost::uintmax_t size = 0;
	for (
		boost::filesystem::recursive_directory_iterator i = boost::filesystem::recursive_directory_iterator(output_dcp);
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

	pair<int, int> ch = audio_channel_types (film()->mapped_audio_channels(), film()->audio_channels());
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
	film()->length().split(film()->video_frame_rate(), h, m, s, fr);
	string length;
	if (h == 0 && m == 0) {
		length = String::compose("%1s", s);
	} else if (h == 0 && m > 0) {
		length = String::compose("%1m%2s", m, s);
	} else if (h > 0 && m > 0) {
		length = String::compose("%1h%2m%3s", h, m, s);
	}

	boost::algorithm::replace_all (text, "$LENGTH", length);

	checked_fwrite (text.c_str(), text.length(), f, cover);
	fclose (f);
}

/** @param frame Frame index within the whole DCP.
 *  @return true if we can fake-write this frame.
 */
bool
Writer::can_fake_write (Frame frame) const
{
	if (film()->encrypted()) {
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

/** @param track Closed caption track if type == TEXT_CLOSED_CAPTION */
void
Writer::write (PlayerText text, TextType type, optional<DCPTextTrack> track, DCPTimePeriod period)
{
	vector<ReelWriter>::iterator* reel = 0;

	switch (type) {
	case TEXT_OPEN_SUBTITLE:
		reel = &_subtitle_reel;
		_have_subtitles = true;
		break;
	case TEXT_CLOSED_CAPTION:
		DCPOMATIC_ASSERT (track);
		DCPOMATIC_ASSERT (_caption_reels.find(*track) != _caption_reels.end());
		reel = &_caption_reels[*track];
		_have_closed_captions.insert (*track);
		break;
	default:
		DCPOMATIC_ASSERT (false);
	}

	DCPOMATIC_ASSERT (*reel != _reels.end());
	while ((*reel)->period().to <= period.from) {
		++(*reel);
		DCPOMATIC_ASSERT (*reel != _reels.end());
	}

	(*reel)->write (text, type, track, period);
}

void
Writer::write (vector<FontData> fonts)
{
	/* Just keep a list of unique fonts and we'll deal with them in ::finish */

	BOOST_FOREACH (FontData const& i, fonts) {
		bool got = false;
		BOOST_FOREACH (FontData const& j, _fonts) {
			if (i == j) {
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
	DCPTime t = DCPTime::from_frames (frame, film()->video_frame_rate());
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
	boost::mutex::scoped_lock lm (_digest_progresses_mutex);

	_digest_progresses[boost::this_thread::get_id()] = progress;
	float min_progress = FLT_MAX;
	for (map<boost::thread::id, float>::const_iterator i = _digest_progresses.begin(); i != _digest_progresses.end(); ++i) {
		min_progress = min (min_progress, i->second);
	}

	job->set_progress (min_progress);

	Waker waker;
	waker.nudge ();
}


/** Calculate hashes for any referenced MXF assets which do not already have one */
void
Writer::calculate_referenced_digests (boost::function<void (float)> set_progress)
{
	BOOST_FOREACH (ReferencedReelAsset const& i, _reel_assets) {
		shared_ptr<dcp::ReelMXF> mxf = dynamic_pointer_cast<dcp::ReelMXF>(i.asset);
		if (mxf && !mxf->hash()) {
			mxf->asset_ref().asset()->hash (set_progress);
			mxf->set_hash (mxf->asset_ref().asset()->hash());
		}
	}
}

