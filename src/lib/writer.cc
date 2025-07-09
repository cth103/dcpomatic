/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


#include "audio_buffers.h"
#include "audio_mapping.h"
#include "compose.hpp"
#include "config.h"
#include "constants.h"
#include "cross.h"
#include "dcp_content_type.h"
#include "dcp_video.h"
#include "dcpomatic_log.h"
#include "film.h"
#include "film_util.h"
#include "frame_info.h"
#include "job.h"
#include "log.h"
#include "ratio.h"
#include "reel_writer.h"
#include "text_content.h"
#include "util.h"
#include "version.h"
#include "writer.h"
#include <dcp/cpl.h>
#include <dcp/mono_mpeg2_picture_frame.h>
#include <dcp/locale_convert.h>
#include <dcp/reel_file_asset.h>
#include <dcp/reel_text_asset.h>
#include <cerrno>
#include <cfloat>
#include <set>

#include "i18n.h"


/* OS X strikes again */
#undef set_key


using std::cout;
using std::dynamic_pointer_cast;
using std::make_shared;
using std::max;
using std::min;
using std::shared_ptr;
using std::set;
using std::string;
using std::vector;
using std::weak_ptr;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using dcp::Data;
using dcp::ArrayData;
using namespace dcpomatic;


/** @param weak_job Job to report progress to, or 0.
 *  @param text_only true to enable only the text (subtitle/ccap) parts of the writer.
 */
Writer::Writer(weak_ptr<const Film> weak_film, weak_ptr<Job> weak_job, boost::filesystem::path output_dir, bool text_only)
	: WeakConstFilm(weak_film)
	, _job(weak_job)
	, _output_dir(output_dir)
	/* These will be reset to sensible values when J2KEncoder is created */
	, _maximum_frames_in_memory (8)
	, _maximum_queue_size (8)
	, _text_only (text_only)
{
	auto job = _job.lock ();

	int reel_index = 0;
	auto const reels = film()->reels();
	for (auto p: reels) {
		_reels.emplace_back(weak_film, p, job, reel_index++, reels.size(), text_only, _output_dir);
	}

	_last_written.resize (reels.size());

	/* We can keep track of the current audio, subtitle and closed caption reels easily because audio
	   and captions arrive to the Writer in sequence.  This is not so for video.
	*/
	_audio_reel = _reels.begin ();
	_subtitle_reel = _reels.begin ();
	for (auto i: film()->closed_text_tracks()) {
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

	if (_zombie) {
		return;
	}

	while (_queued_full_in_memory > _maximum_frames_in_memory) {
		/* There are too many full frames in memory; wake the main writer thread and
		   wait until it sorts everything out */
		_empty_condition.notify_all ();
		_full_condition.wait (lock);
	}

	QueueItem qi;
	qi.type = QueueItem::Type::FULL;
	qi.encoded = encoded;
	qi.reel = video_reel (frame);
	qi.frame = frame - _reels[qi.reel].start ();

	DCPOMATIC_ASSERT((film()->three_d() && eyes != Eyes::BOTH) || (!film()->three_d() && eyes == Eyes::BOTH));

	qi.eyes = eyes;
	_queue.push_back(qi);
	++_queued_full_in_memory;

	/* Now there's something to do: wake anything wait()ing on _empty_condition */
	_empty_condition.notify_all ();
}


void
Writer::write(shared_ptr<dcp::MonoMPEG2PictureFrame> image, Frame frame)
{
	_reels[video_reel(frame)].write(image);
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
	qi.type = QueueItem::Type::REPEAT;
	qi.reel = video_reel (frame);
	qi.frame = frame - _reels[qi.reel].start ();
	if (film()->three_d() && eyes == Eyes::BOTH) {
		qi.eyes = Eyes::LEFT;
		_queue.push_back (qi);
		qi.eyes = Eyes::RIGHT;
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

	DCPOMATIC_ASSERT((film()->three_d() && eyes != Eyes::BOTH) || (!film()->three_d() && eyes == Eyes::BOTH));

	QueueItem qi;
	qi.type = QueueItem::Type::FAKE;
	auto const reel = video_reel(frame);
	qi.reel = reel;
	qi.frame = frame - _reels[reel].start();
	qi.eyes = eyes;
	_queue.push_back(qi);

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

			/* Be careful that part_lengths[0] + part_lengths[1] can't be bigger than audio->frames() */
			Frame part_frames[2] = {
				part_lengths[0].frames_ceil(afr),
				part_lengths[1].frames_floor(afr)
			};

			DCPOMATIC_ASSERT ((part_frames[0] + part_frames[1]) <= audio->frames());

			if (part_frames[0]) {
				auto part = make_shared<AudioBuffers>(audio, part_frames[0], 0);
				_audio_reel->write (part);
			}

			if (part_frames[1]) {
				audio = make_shared<AudioBuffers>(audio, part_frames[1], part_frames[0]);
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
	auto const & f = _queue.front();
	return _last_written[f.reel].next(f);
}


bool
Writer::LastWritten::next (QueueItem qi) const
{
	if (qi.eyes == Eyes::BOTH) {
		/* 2D */
		return qi.frame == (_frame + 1);
	}

	/* 3D */

	if (_eyes == Eyes::LEFT && qi.frame == _frame && qi.eyes == Eyes::RIGHT) {
		return true;
	}

	if (_eyes == Eyes::RIGHT && qi.frame == (_frame + 1) && qi.eyes == Eyes::LEFT) {
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
	start_of_thread ("Writer");

	while (true)
	{
		boost::mutex::scoped_lock lock (_state_mutex);
		if (_zombie) {
			return;
		}

		while (true) {

			if (_finish || _queued_full_in_memory > _maximum_frames_in_memory || have_sequenced_image_at_queue_head ()) {
				/* We've got something to do: go and do it */
				break;
			}

			/* Nothing to do: wait until something happens which may indicate that we do */
			LOG_TIMING (N_("writer-sleep queue={}"), _queue.size());
			_empty_condition.wait (lock);
			LOG_TIMING (N_("writer-wake queue={}"), _queue.size());
		}

		/* We stop here if we have been asked to finish, and if either the queue
		   is empty or we do not have a sequenced image at its head (if this is the
		   case we will never terminate as no new frames will be sent once
		   _finish is true).
		*/
		if (_finish && (!have_sequenced_image_at_queue_head() || _queue.empty())) {
			/* (Hopefully temporarily) log anything that was not written */
			if (!_queue.empty() && !have_sequenced_image_at_queue_head()) {
				LOG_WARNING (N_("Finishing writer with a left-over queue of {}:"), _queue.size());
				for (auto const& i: _queue) {
					if (i.type == QueueItem::Type::FULL) {
						LOG_WARNING (N_("- type FULL, frame {}, eyes {}"), i.frame, (int) i.eyes);
					} else {
						LOG_WARNING(N_("- type FAKE, frame {}, eyes {}"), i.frame, static_cast<int>(i.eyes));
					}
				}
			}
			return;
		}

		/* Write any frames that we can write; i.e. those that are in sequence. */
		while (have_sequenced_image_at_queue_head ()) {
			auto qi = _queue.front ();
			_last_written[qi.reel].update (qi);
			_queue.pop_front ();
			if (qi.encoded) {
				--_queued_full_in_memory;
			}

			lock.unlock ();

			auto& reel = _reels[qi.reel];

			switch (qi.type) {
			case QueueItem::Type::FULL:
				LOG_DEBUG_ENCODE (N_("Writer FULL-writes {} ({})"), qi.frame, (int) qi.eyes);
				if (!qi.encoded) {
					/* Get the data back from disk where we stored it temporarily */
					qi.encoded = make_shared<ArrayData>(film()->j2c_path(qi.reel, qi.frame, qi.eyes, false));
				}
				reel.write (qi.encoded, qi.frame, qi.eyes);
				++_full_written;
				break;
			case QueueItem::Type::FAKE:
				LOG_DEBUG_ENCODE (N_("Writer FAKE-writes {}"), qi.frame);
				reel.fake_write(qi.frame, qi.eyes);
				++_fake_written;
				break;
			case QueueItem::Type::REPEAT:
				LOG_DEBUG_ENCODE (N_("Writer REPEAT-writes {}"), qi.frame);
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
			auto item = _queue.rbegin();
			while (item != _queue.rend() && !item->encoded) {
				++item;
			}

			DCPOMATIC_ASSERT(item != _queue.rend());
			++_pushed_to_disk;

			LOG_GENERAL("Writer full; pushes {} to disk while awaiting {}", item->frame, _last_written[_queue.front().reel].frame() + 1);

			item->encoded->write_via_temp(
				film()->j2c_path(item->reel, item->frame, item->eyes, true),
				film()->j2c_path(item->reel, item->frame, item->eyes, false)
				);

			item->encoded.reset();
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


void
Writer::calculate_digests ()
{
	auto job = _job.lock ();
	if (job) {
		job->sub (_("Computing digests"));
	}

	dcpomatic::io_context context;
	boost::thread_group pool;

	{
		auto work = dcpomatic::make_work_guard(context);

		int const threads = max (1, Config::instance()->master_encoding_threads());

		for (int i = 0; i < threads; ++i) {
			pool.create_thread(boost::bind(&dcpomatic::io_context::run, &context));
		}

		std::function<void (int, int64_t, int64_t)> set_progress;
		if (job) {
			set_progress = boost::bind(&Writer::set_digest_progress, this, job.get(), _1, _2, _3);
		} else {
			set_progress = [](int, int64_t, int64_t) {
				boost::this_thread::interruption_point();
			};
		}

		int index = 0;

		for (auto& i: _reels) {
			dcpomatic::post(context, boost::bind(
					&ReelWriter::calculate_digests,
					&i,
					std::function<void (int64_t, int64_t)>(boost::bind(set_progress, index, _1, _2))
					));
			++index;
		}
		dcpomatic::post(context, boost::bind(
				&Writer::calculate_referenced_digests,
				this,
				std::function<void (int64_t, int64_t)>(boost::bind(set_progress, index, _1, _2))
				));
	}

	try {
		pool.join_all ();
	} catch (boost::thread_interrupted) {
		/* join_all was interrupted, so we need to interrupt the threads
		 * in our pool then try again to join them.
		 */
		pool.interrupt_all ();
		pool.join_all ();
	}

	context.stop();
}


void
Writer::finish()
{
	if (_thread.joinable()) {
		LOG_GENERAL_NC ("Terminating writer thread");
		terminate_thread (true);
	}

	LOG_GENERAL_NC ("Finishing ReelWriters");

	for (auto& reel: _reels) {
		write_hanging_text(reel);
		reel.finish(_output_dir);
	}

	LOG_GENERAL_NC ("Writing XML");

	dcp::DCP dcp(_output_dir);

	auto cpl = make_shared<dcp::CPL>(
		film()->dcp_name(),
		film()->dcp_content_type()->libdcp_kind(),
		film()->interop() ? dcp::Standard::INTEROP : dcp::Standard::SMPTE,
		film()->limit_to_smpte_bv20() ? dcp::Profile::SMPTE_BV20 : dcp::Profile::SMPTE_BV21
		);

	dcp.add (cpl);

	calculate_digests ();

	/* Add reels */

	for (auto& i: _reels) {
		cpl->add(i.create_reel(_reel_assets, _output_dir, _have_subtitles, _have_closed_captions));
	}

	/* Add metadata */

	auto creator = Config::instance()->dcp_creator();
	if (creator.empty()) {
		creator = fmt::format("DCP-o-matic {} {}", dcpomatic_version, dcpomatic_git_commit);
	}

	auto issuer = Config::instance()->dcp_issuer();
	if (issuer.empty()) {
		issuer = fmt::format("DCP-o-matic {} {}", dcpomatic_version, dcpomatic_git_commit);
	}

	cpl->set_creator (creator);
	cpl->set_issuer (issuer);

	cpl->set_ratings (film()->ratings());

	vector<dcp::ContentVersion> cv;
	for (auto i: film()->content_versions()) {
		/* Make sure we don't end up writing an empty <LabelText> node as some validators
		 * complain about that.
		 */
		cv.push_back(!i.empty() ? dcp::ContentVersion(i) : dcp::ContentVersion("1"));
	}
	if (cv.empty()) {
		cv = { dcp::ContentVersion("1") };
	}
	cpl->set_content_versions (cv);

	cpl->set_full_content_title_text (film()->name());
	cpl->set_full_content_title_text_language (film()->name_language());
	if (film()->release_territory()) {
		cpl->set_release_territory (*film()->release_territory());
	}
	cpl->set_version_number (film()->version_number());
	cpl->set_status (film()->status());
	if (film()->chain()) {
		cpl->set_chain (*film()->chain());
	}
	if (film()->distributor()) {
		cpl->set_distributor (*film()->distributor());
	}
	if (film()->facility()) {
		cpl->set_facility (*film()->facility());
	}
	if (film()->luminance()) {
		cpl->set_luminance (*film()->luminance());
	}
	if (film()->sign_language_video_language()) {
		cpl->set_sign_language_video_language (*film()->sign_language_video_language());
	}

	dcp::MCASoundField field;
	if (dcpomatic::film::channel_is_mapped(film(), dcp::Channel::BSL) || dcpomatic::film::channel_is_mapped(film(), dcp::Channel::BSR)) {
		field = dcp::MCASoundField::SEVEN_POINT_ONE;
	} else {
		field = dcp::MCASoundField::FIVE_POINT_ONE;
	}

	auto const audio_channels = film()->audio_channels();
	dcp::MainSoundConfiguration msc(field, audio_channels);
	for (auto i: film()->mapped_audio_channels()) {
		if (i < audio_channels) {
			msc.set_mapping(i, static_cast<dcp::Channel>(i));
		}
	}

	cpl->set_main_sound_configuration(msc);
	cpl->set_main_sound_sample_rate (film()->audio_frame_rate());
	cpl->set_main_picture_stored_area (film()->frame_size());

	auto active_area = film()->active_area();
	if (active_area.width > 0 && active_area.height > 0) {
		/* It's not allowed to have a zero active area width or height, and the sizes must be multiples of 2 */
		cpl->set_main_picture_active_area({ active_area.width & ~1, active_area.height & ~1});
	}

	auto const sl = film()->open_text_languages().second;
	if (!sl.empty()) {
		cpl->set_additional_subtitle_languages(sl);
	}

	auto signer = Config::instance()->signer_chain();
	/* We did check earlier, but check again here to be on the safe side */
	string reason;
	if (!signer->valid (&reason)) {
		throw InvalidSignerError (reason);
	}

	dcp.set_issuer(issuer);
	dcp.set_creator(creator);
	dcp.set_annotation_text(film()->dcp_name());

	optional<string> group_id;
	if (dcpomatic::film::is_vf(film())) {
		group_id = dcp::make_uuid();
	}
	dcp.write_xml(signer, Config::instance()->dcp_metadata_filename_format(), group_id);

	LOG_GENERAL (
		N_("Wrote {} FULL, {} FAKE, {} REPEAT, {} pushed to disk"), _full_written, _fake_written, _repeat_written, _pushed_to_disk
		);

	write_cover_sheet();
}


void
Writer::write_cover_sheet()
{
	auto const cover = film()->file("COVER_SHEET.txt");
	dcp::File file(cover, "w");
	if (!file) {
		throw OpenFileError(cover, file.open_error(), OpenFileError::WRITE);
	}

	auto text = Config::instance()->cover_sheet ();
	boost::algorithm::replace_all (text, "$CPL_NAME", film()->name());
	auto cpls = film()->cpls();
	if (!cpls.empty()) {
		boost::algorithm::replace_all (text, "$CPL_FILENAME", cpls[0].cpl_file.filename().string());
	}
	boost::algorithm::replace_all(text, "$TYPE", film()->dcp_content_type()->pretty_name());
	boost::algorithm::replace_all(text, "$CONTAINER", film()->container().container_nickname());

	auto audio_language = film()->audio_language();
	if (audio_language) {
		boost::algorithm::replace_all (text, "$AUDIO_LANGUAGE", audio_language->description());
	} else {
		boost::algorithm::replace_all (text, "$AUDIO_LANGUAGE", _("None"));
	}

	auto const subtitle_languages = film()->open_text_languages();
	if (subtitle_languages.first) {
		boost::algorithm::replace_all (text, "$SUBTITLE_LANGUAGE", subtitle_languages.first->description());
	} else {
		boost::algorithm::replace_all (text, "$SUBTITLE_LANGUAGE", _("None"));
	}

	boost::uintmax_t size = 0;
	for (
		auto i = dcp::filesystem::recursive_directory_iterator(_output_dir);
		i != dcp::filesystem::recursive_directory_iterator();
		++i) {
		if (dcp::filesystem::is_regular_file(i->path())) {
			size += dcp::filesystem::file_size(i->path());
		}
	}

	if (size > (1000000000L)) {
		boost::algorithm::replace_all (text, "$SIZE", fmt::format("{}GB", dcp::locale_convert<string>(size / 1000000000.0, 1, true)));
	} else {
		boost::algorithm::replace_all (text, "$SIZE", fmt::format("{}MB", dcp::locale_convert<string>(size / 1000000.0, 1, true)));
	}

	auto ch = audio_channel_types (film()->mapped_audio_channels(), film()->audio_channels());
	auto description = fmt::format("{}.{}", ch.first, ch.second);

	if (description == "0.0") {
		description = _("None");
	} else if (description == "1.0") {
		description = _("Mono");
	} else if (description == "2.0") {
		description = _("Stereo");
	}
	boost::algorithm::replace_all (text, "$AUDIO", description);

	auto const hmsf = film()->length().split(film()->video_frame_rate());
	string length;
	if (hmsf.h == 0 && hmsf.m == 0) {
		length = fmt::format("{}s", hmsf.s);
	} else if (hmsf.h == 0 && hmsf.m > 0) {
		length = fmt::format("{}m{}s", hmsf.m, hmsf.s);
	} else if (hmsf.h > 0 && hmsf.m > 0) {
		length = fmt::format("{}h{}m{}s", hmsf.h, hmsf.m, hmsf.s);
	}

	boost::algorithm::replace_all (text, "$LENGTH", length);

	file.checked_write(text.c_str(), text.length());
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

	auto const & reel = _reels[video_reel(frame)];

	/* Make frame relative to the start of the reel */
	frame -= reel.start ();
	return (frame != 0 && frame < reel.first_nonexistent_frame());
}


/** @param track Closed caption track if type == TextType::CLOSED_CAPTION */
void
Writer::write (PlayerText text, TextType type, optional<DCPTextTrack> track, DCPTimePeriod period)
{
	vector<ReelWriter>::iterator* reel = nullptr;

	switch (type) {
	case TextType::OPEN_SUBTITLE:
	case TextType::OPEN_CAPTION:
		reel = &_subtitle_reel;
		_have_subtitles = true;
		break;
	case TextType::CLOSED_CAPTION:
	case TextType::CLOSED_SUBTITLE:
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
		write_hanging_text (**reel);
	}

	auto back_off = [this](DCPTimePeriod period) {
		auto const vfr = film()->video_frame_rate();
		period.to -= DCPTime::from_frames(2, vfr);
		if (period.duration().frames_floor(vfr) <= 0) {
			period.to = period.from + DCPTime::from_frames(1, vfr);
		}
		return period;
	};

	if (period.to > (*reel)->period().to) {
		/* This text goes off the end of the reel.  Store parts of it that should go into
		 * other reels.
		 */
		for (auto i = std::next(*reel); i != _reels.end(); ++i) {
			auto overlap = i->period().overlap(period);
			if (overlap) {
				_hanging_texts.push_back (HangingText{text, type, track, back_off(*overlap)});
			}
		}
		/* Try to back off from the reel boundary by a couple of frames to avoid tripping checks
		 * for subtitles being too close together.
		 */
		period.to = (*reel)->period().to;
		period = back_off(period);
	}

	(*reel)->write(text, type, track, period, _fonts, _chosen_interop_font);
}


void
Writer::write (vector<shared_ptr<Font>> fonts)
{
	if (fonts.empty()) {
		return;
	}

	/* Fonts may come in with empty IDs but we don't want to put those in the DCP */
	auto fix_id = [](string id) {
		return id.empty() ? "font" : id;
	};

	if (film()->interop()) {
		/* Interop will ignore second and subsequent <LoadFont>s so we don't want to
		 * even write them as they upset some validators.  Set up _fonts so that every
		 * font used by any subtitle will be written with the same ID.
		 */
		for (size_t i = 0; i < fonts.size(); ++i) {
			_fonts.put(fonts[i], fix_id(fonts[0]->id()));
		}
		_chosen_interop_font = fonts[0];
	} else {
		for (auto font: fonts) {
			_fonts.put(font, fix_id(font->id()));
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

	if (auto text_asset = dynamic_pointer_cast<dcp::ReelTextAsset>(asset.asset)) {
		if (is_open(text_asset->type())) {
			_have_subtitles = true;
		} else {
			/* This feels quite fragile. We have a referenced reel and want to know if it's
			 * part of a given closed-caption track so that we can fill if it has any
			 * missing reels.  I guess for that purpose almost any DCPTextTrack values are
			 * fine so long as they are consistent.
			 */
			DCPTextTrack track;
			track.name = text_asset->annotation_text().get_value_or("");
			track.language = dcp::LanguageTag(text_asset->language().get_value_or("en-US"));
			if (_have_closed_captions.find(track) == _have_closed_captions.end()) {
				_have_closed_captions.insert(track);
			}
		}
	}
}


size_t
Writer::video_reel (int frame) const
{
	auto t = DCPTime::from_frames (frame, film()->video_frame_rate());
	size_t reel_index = 0;
	while (reel_index < _reels.size() && !_reels[reel_index].period().contains(t)) {
		++reel_index;
	}

	DCPOMATIC_ASSERT(reel_index < _reels.size ());
	return reel_index;
}


/** Update job progress with information about the progress of a single digest calculation
 *  thread.
 *  @param id Unique identifier for the thread whose progress has changed.
 *  @param done Number of bytes that this thread has processed.
 *  @param size Total number of bytes that this thread must process.
 */
void
Writer::set_digest_progress(Job* job, int id, int64_t done, int64_t size)
{
	boost::mutex::scoped_lock lm (_digest_progresses_mutex);

	/* Update the progress for this thread */
	_digest_progresses[id] = std::make_pair(done, size);

	/* Get the total progress across all threads and use it to set job progress */
	int64_t total_done = 0;
	int64_t total_size = 0;
	for (auto const& i: _digest_progresses) {
		total_done += i.second.first;
		total_size += i.second.second;
	}

	job->set_progress(float(total_done) / total_size);

	Waker waker;
	waker.nudge ();

	boost::this_thread::interruption_point();
}


/** Calculate hashes for any referenced MXF assets which do not already have one */
void
Writer::calculate_referenced_digests(std::function<void (int64_t, int64_t)> set_progress)
try
{
	int64_t total_size = 0;
	for (auto const& i: _reel_assets) {
		auto file = dynamic_pointer_cast<dcp::ReelFileAsset>(i.asset);
		if (file && !file->hash()) {
			auto filename = file->asset_ref().asset()->file();
			DCPOMATIC_ASSERT(filename);
			total_size += boost::filesystem::file_size(*filename);
		}
	}

	int64_t total_done = 0;
	for (auto const& i: _reel_assets) {
		auto file = dynamic_pointer_cast<dcp::ReelFileAsset>(i.asset);
		if (file && !file->hash()) {
			file->asset_ref().asset()->hash([&total_done, total_size, set_progress](int64_t done, int64_t) {
				set_progress(total_done + done, total_size);
			});
			total_done += boost::filesystem::file_size(*file->asset_ref().asset()->file());
			file->set_hash (file->asset_ref().asset()->hash());
		}
	}
} catch (boost::thread_interrupted) {
	/* set_progress contains an interruption_point, so any of these methods
	 * may throw thread_interrupted, at which point we just give up.
	 */
}


void
Writer::write_hanging_text (ReelWriter& reel)
{
	vector<HangingText> new_hanging_texts;
	for (auto i: _hanging_texts) {
		if (i.period.from == reel.period().from) {
			reel.write(i.text, i.type, i.track, i.period, _fonts, _chosen_interop_font);
		} else {
			new_hanging_texts.push_back (i);
		}
	}
	_hanging_texts = new_hanging_texts;
}


/** Set the writer so that it has no queue and drops any pending or future requests to write images */
void
Writer::zombify()
{
	boost::mutex::scoped_lock lock(_state_mutex);

	_queue.clear();
	_queued_full_in_memory = 0;
	_zombie = true;
	_full_condition.notify_all();
}

