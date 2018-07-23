/*
    Copyright (C) 2012-2018 Carl Hetherington <cth@carlh.net>

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

#include "reel_writer.h"
#include "film.h"
#include "cross.h"
#include "job.h"
#include "log.h"
#include "digester.h"
#include "font.h"
#include "compose.hpp"
#include "audio_buffers.h"
#include "image.h"
#include <dcp/mono_picture_asset.h>
#include <dcp/stereo_picture_asset.h>
#include <dcp/sound_asset.h>
#include <dcp/sound_asset_writer.h>
#include <dcp/reel.h>
#include <dcp/reel_mono_picture_asset.h>
#include <dcp/reel_stereo_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/reel_subtitle_asset.h>
#include <dcp/reel_closed_caption_asset.h>
#include <dcp/dcp.h>
#include <dcp/cpl.h>
#include <dcp/certificate_chain.h>
#include <dcp/interop_subtitle_asset.h>
#include <dcp/smpte_subtitle_asset.h>
#include <dcp/raw_convert.h>
#include <dcp/subtitle_image.h>
#include <boost/foreach.hpp>

#include "i18n.h"

#define LOG_GENERAL(...) _film->log()->log (String::compose (__VA_ARGS__), LogEntry::TYPE_GENERAL);
#define LOG_GENERAL_NC(...) _film->log()->log (__VA_ARGS__, LogEntry::TYPE_GENERAL);
#define LOG_WARNING_NC(...) _film->log()->log (__VA_ARGS__, LogEntry::TYPE_WARNING);
#define LOG_ERROR(...) _film->log()->log (String::compose (__VA_ARGS__), LogEntry::TYPE_ERROR);

using std::list;
using std::string;
using std::cout;
using boost::shared_ptr;
using boost::optional;
using boost::dynamic_pointer_cast;
using dcp::Data;
using dcp::raw_convert;

int const ReelWriter::_info_size = 48;

ReelWriter::ReelWriter (
	shared_ptr<const Film> film, DCPTimePeriod period, shared_ptr<Job> job, int reel_index, int reel_count, optional<string> content_summary
	)
	: _film (film)
	, _period (period)
	, _last_written_video_frame (-1)
	, _last_written_eyes (EYES_RIGHT)
	, _reel_index (reel_index)
	, _reel_count (reel_count)
	, _content_summary (content_summary)
{
	/* Create our picture asset in a subdirectory, named according to those
	   film's parameters which affect the video output.  We will hard-link
	   it into the DCP later.
	*/

	dcp::Standard const standard = _film->interop() ? dcp::INTEROP : dcp::SMPTE;

	if (_film->three_d ()) {
		_picture_asset.reset (new dcp::StereoPictureAsset (dcp::Fraction (_film->video_frame_rate(), 1), standard));
	} else {
		_picture_asset.reset (new dcp::MonoPictureAsset (dcp::Fraction (_film->video_frame_rate(), 1), standard));
	}

	_picture_asset->set_size (_film->frame_size ());

	if (_film->encrypted ()) {
		_picture_asset->set_key (_film->key ());
		_picture_asset->set_context_id (_film->context_id ());
	}

	_picture_asset->set_file (
		_film->internal_video_asset_dir() / _film->internal_video_asset_filename(_period)
		);

	job->sub (_("Checking existing image data"));
	_first_nonexistant_frame = check_existing_picture_asset ();

	_picture_asset_writer = _picture_asset->start_write (
		_film->internal_video_asset_dir() / _film->internal_video_asset_filename(_period),
		_first_nonexistant_frame > 0
		);

	if (_film->audio_channels ()) {
		_sound_asset.reset (
			new dcp::SoundAsset (dcp::Fraction (_film->video_frame_rate(), 1), _film->audio_frame_rate (), _film->audio_channels (), standard)
			);

		if (_film->encrypted ()) {
			_sound_asset->set_key (_film->key ());
		}

		DCPOMATIC_ASSERT (_film->directory());

		/* Write the sound asset into the film directory so that we leave the creation
		   of the DCP directory until the last minute.
		*/
		_sound_asset_writer = _sound_asset->start_write (
			_film->directory().get() / audio_asset_filename (_sound_asset, _reel_index, _reel_count, _content_summary)
			);
	}
}

/** @param frame reel-relative frame */
void
ReelWriter::write_frame_info (Frame frame, Eyes eyes, dcp::FrameInfo info) const
{
	FILE* file = 0;
	boost::filesystem::path info_file = _film->info_file (_period);

	bool const read = boost::filesystem::exists (info_file);

	if (read) {
		file = fopen_boost (info_file, "r+b");
	} else {
		file = fopen_boost (info_file, "wb");
	}
	if (!file) {
		throw OpenFileError (info_file, errno, read);
	}
	dcpomatic_fseek (file, frame_info_position (frame, eyes), SEEK_SET);
	fwrite (&info.offset, sizeof (info.offset), 1, file);
	fwrite (&info.size, sizeof (info.size), 1, file);
	fwrite (info.hash.c_str(), 1, info.hash.size(), file);
	fclose (file);
}

dcp::FrameInfo
ReelWriter::read_frame_info (FILE* file, Frame frame, Eyes eyes) const
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

long
ReelWriter::frame_info_position (Frame frame, Eyes eyes) const
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

Frame
ReelWriter::check_existing_picture_asset ()
{
	DCPOMATIC_ASSERT (_picture_asset->file());
	boost::filesystem::path asset = _picture_asset->file().get();

	/* If there is an existing asset, break any hard links to it as we are about to change its contents
	   (if only by changing the IDs); see #1126.
	*/

	if (boost::filesystem::exists(asset) && boost::filesystem::hard_link_count(asset) > 1) {
		boost::filesystem::copy_file (asset, asset.string() + ".tmp");
		boost::filesystem::remove (asset);
		boost::filesystem::rename (asset.string() + ".tmp", asset);
	}

	/* Try to open the existing asset */
	FILE* asset_file = fopen_boost (asset, "rb");
	if (!asset_file) {
		LOG_GENERAL ("Could not open existing asset at %1 (errno=%2)", asset.string(), errno);
		return 0;
	} else {
		LOG_GENERAL ("Opened existing asset at %1", asset.string());
	}

	/* Offset of the last dcp::FrameInfo in the info file */
	int const n = (boost::filesystem::file_size (_film->info_file(_period)) / _info_size) - 1;
	LOG_GENERAL ("The last FI is %1; info file is %2, info size %3", n, boost::filesystem::file_size (_film->info_file(_period)), _info_size);

	FILE* info_file = fopen_boost (_film->info_file(_period), "rb");
	if (!info_file) {
		LOG_GENERAL_NC ("Could not open film info file");
		fclose (asset_file);
		return 0;
	}

	Frame first_nonexistant_frame;
	if (_film->three_d ()) {
		/* Start looking at the last left frame */
		first_nonexistant_frame = n / 2;
	} else {
		first_nonexistant_frame = n;
	}

	while (!existing_picture_frame_ok(asset_file, info_file, first_nonexistant_frame) && first_nonexistant_frame > 0) {
		--first_nonexistant_frame;
	}

	if (!_film->three_d() && first_nonexistant_frame > 0) {
		/* If we are doing 3D we might have found a good L frame with no R, so only
		   do this if we're in 2D and we've just found a good B(oth) frame.
		*/
		++first_nonexistant_frame;
	}

	LOG_GENERAL ("Proceeding with first nonexistant frame %1", first_nonexistant_frame);

	fclose (asset_file);
	fclose (info_file);

	return first_nonexistant_frame;
}

void
ReelWriter::write (optional<Data> encoded, Frame frame, Eyes eyes)
{
	dcp::FrameInfo fin = _picture_asset_writer->write (encoded->data().get (), encoded->size());
	write_frame_info (frame, eyes, fin);
	_last_written[eyes] = encoded;
	_last_written_video_frame = frame;
	_last_written_eyes = eyes;
}

void
ReelWriter::fake_write (Frame frame, Eyes eyes, int size)
{
	_picture_asset_writer->fake_write (size);
	_last_written_video_frame = frame;
	_last_written_eyes = eyes;
}

void
ReelWriter::repeat_write (Frame frame, Eyes eyes)
{
	dcp::FrameInfo fin = _picture_asset_writer->write (
		_last_written[eyes]->data().get(),
		_last_written[eyes]->size()
		);
	write_frame_info (frame, eyes, fin);
	_last_written_video_frame = frame;
	_last_written_eyes = eyes;
}

void
ReelWriter::finish ()
{
	if (!_picture_asset_writer->finalize ()) {
		/* Nothing was written to the picture asset */
		LOG_GENERAL ("Nothing was written to reel %1 of %2", _reel_index, _reel_count);
		_picture_asset.reset ();
	}

	if (_sound_asset_writer && !_sound_asset_writer->finalize ()) {
		/* Nothing was written to the sound asset */
		_sound_asset.reset ();
	}

	/* Hard-link any video asset file into the DCP */
	if (_picture_asset) {
		DCPOMATIC_ASSERT (_picture_asset->file());
		boost::filesystem::path video_from = _picture_asset->file().get();
		boost::filesystem::path video_to;
		video_to /= _film->dir (_film->dcp_name());
		video_to /= video_asset_filename (_picture_asset, _reel_index, _reel_count, _content_summary);

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
	}

	/* Move the audio asset into the DCP */
	if (_sound_asset) {
		boost::filesystem::path audio_to;
		audio_to /= _film->dir (_film->dcp_name ());
		string const aaf = audio_asset_filename (_sound_asset, _reel_index, _reel_count, _content_summary);
		audio_to /= aaf;

		boost::system::error_code ec;
		boost::filesystem::rename (_film->file (aaf), audio_to, ec);
		if (ec) {
			throw FileError (
				String::compose (_("could not move audio asset into the DCP (%1)"), ec.value ()), aaf
				);
		}

		_sound_asset->set_file (audio_to);
	}
}

template <class T>
void
maybe_add_captions (
	shared_ptr<dcp::SubtitleAsset> asset,
	int64_t picture_duration,
	shared_ptr<dcp::Reel> reel,
	list<ReferencedReelAsset> const & refs,
	list<shared_ptr<Font> > const & fonts,
	shared_ptr<const Film> film,
	DCPTimePeriod period
	)
{
	Frame const period_duration = period.duration().frames_round(film->video_frame_rate());

	shared_ptr<T> reel_asset;

	if (asset) {

		boost::filesystem::path liberation_normal;
		try {
			liberation_normal = shared_path() / "LiberationSans-Regular.ttf";
			if (!boost::filesystem::exists (liberation_normal)) {
				/* Hack for unit tests */
				liberation_normal = shared_path() / "fonts" / "LiberationSans-Regular.ttf";
			}
		} catch (boost::filesystem::filesystem_error& e) {

		}

		if (!boost::filesystem::exists(liberation_normal)) {
			liberation_normal = "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf";
		}

		/* Add all the fonts to the subtitle content */
		BOOST_FOREACH (shared_ptr<Font> j, fonts) {
			asset->add_font (j->id(), j->file(FontFiles::NORMAL).get_value_or(liberation_normal));
		}

		if (dynamic_pointer_cast<dcp::InteropSubtitleAsset> (asset)) {
			boost::filesystem::path directory = film->dir (film->dcp_name ()) / asset->id ();
			boost::filesystem::create_directories (directory);
			asset->write (directory / ("sub_" + asset->id() + ".xml"));
		} else {

			/* All our assets should be the same length; use the picture asset length here
			   as a reference to set the subtitle one.  We'll use the duration rather than
			   the intrinsic duration; we don't care if the picture asset has been trimmed, we're
			   just interested in its presentation length.
			*/
			dynamic_pointer_cast<dcp::SMPTESubtitleAsset>(asset)->set_intrinsic_duration (picture_duration);

			asset->write (
				film->dir(film->dcp_name()) / ("sub_" + asset->id() + ".mxf")
				);
		}

		reel_asset.reset (
			new T (
				asset,
				dcp::Fraction (film->video_frame_rate(), 1),
				picture_duration,
				0
				)
			);
	} else {
		/* We don't have a subtitle asset of our own; hopefully we have one to reference */
		BOOST_FOREACH (ReferencedReelAsset j, refs) {
			shared_ptr<T> k = dynamic_pointer_cast<T> (j.asset);
			if (k && j.period == period) {
				reel_asset = k;
				/* If we have a hash for this asset in the CPL, assume that it is correct */
				if (k->hash()) {
					k->asset_ref()->set_hash (k->hash().get());
				}
			}
		}
	}

	if (reel_asset) {
		if (reel_asset->duration() != period_duration) {
			throw ProgrammingError (
				__FILE__, __LINE__,
				String::compose ("%1 vs %2", reel_asset->duration(), period_duration)
				);
		}
		reel->add (reel_asset);
	}
}

shared_ptr<dcp::Reel>
ReelWriter::create_reel (list<ReferencedReelAsset> const & refs, list<shared_ptr<Font> > const & fonts)
{
	shared_ptr<dcp::Reel> reel (new dcp::Reel ());

	shared_ptr<dcp::ReelPictureAsset> reel_picture_asset;

	if (_picture_asset) {
		/* We have made a picture asset of our own.  Put it into the reel */
		shared_ptr<dcp::MonoPictureAsset> mono = dynamic_pointer_cast<dcp::MonoPictureAsset> (_picture_asset);
		if (mono) {
			reel_picture_asset.reset (new dcp::ReelMonoPictureAsset (mono, 0));
		}

		shared_ptr<dcp::StereoPictureAsset> stereo = dynamic_pointer_cast<dcp::StereoPictureAsset> (_picture_asset);
		if (stereo) {
			reel_picture_asset.reset (new dcp::ReelStereoPictureAsset (stereo, 0));
		}
	} else {
		LOG_GENERAL ("no picture asset of our own; look through %1", refs.size());
		/* We don't have a picture asset of our own; hopefully we have one to reference */
		BOOST_FOREACH (ReferencedReelAsset j, refs) {
			shared_ptr<dcp::ReelPictureAsset> k = dynamic_pointer_cast<dcp::ReelPictureAsset> (j.asset);
			if (k) {
				LOG_GENERAL ("candidate picture asset period is %1-%2", j.period.from.get(), j.period.to.get());
			}
			if (k && j.period == _period) {
				reel_picture_asset = k;
			}
		}
	}

	LOG_GENERAL ("create_reel for %1-%2; %3 of %4", _period.from.get(), _period.to.get(), _reel_index, _reel_count);

	Frame const period_duration = _period.duration().frames_round(_film->video_frame_rate());

	DCPOMATIC_ASSERT (reel_picture_asset);
	if (reel_picture_asset->duration() != period_duration) {
		throw ProgrammingError (
			__FILE__, __LINE__,
			String::compose ("%1 vs %2", reel_picture_asset->duration(), period_duration)
			);
	}
	reel->add (reel_picture_asset);

	/* If we have a hash for this asset in the CPL, assume that it is correct */
	if (reel_picture_asset->hash()) {
		reel_picture_asset->asset_ref()->set_hash (reel_picture_asset->hash().get());
	}

	shared_ptr<dcp::ReelSoundAsset> reel_sound_asset;

	if (_sound_asset) {
		/* We have made a sound asset of our own.  Put it into the reel */
		reel_sound_asset.reset (new dcp::ReelSoundAsset (_sound_asset, 0));
	} else {
		/* We don't have a sound asset of our own; hopefully we have one to reference */
		BOOST_FOREACH (ReferencedReelAsset j, refs) {
			shared_ptr<dcp::ReelSoundAsset> k = dynamic_pointer_cast<dcp::ReelSoundAsset> (j.asset);
			if (k && j.period == _period) {
				reel_sound_asset = k;
				/* If we have a hash for this asset in the CPL, assume that it is correct */
				if (k->hash()) {
					k->asset_ref()->set_hash (k->hash().get());
				}
			}
		}
	}

	DCPOMATIC_ASSERT (reel_sound_asset);
	if (reel_sound_asset->duration() != period_duration) {
		LOG_ERROR (
			"Reel sound asset has length %1 but reel period is %2",
			reel_sound_asset->duration(),
			period_duration
			);
		if (reel_sound_asset->duration() != period_duration) {
			throw ProgrammingError (
				__FILE__, __LINE__,
				String::compose ("%1 vs %2", reel_sound_asset->duration(), period_duration)
				);
		}

	}
	reel->add (reel_sound_asset);

	maybe_add_captions<dcp::ReelSubtitleAsset>      (_caption_asset[TEXT_OPEN_SUBTITLE],   reel_picture_asset->duration(), reel, refs, fonts, _film, _period);
	maybe_add_captions<dcp::ReelClosedCaptionAsset> (_caption_asset[TEXT_CLOSED_CAPTION], reel_picture_asset->duration(), reel, refs, fonts, _film, _period);

	return reel;
}

void
ReelWriter::calculate_digests (boost::function<void (float)> set_progress)
{
	if (_picture_asset) {
		_picture_asset->hash (set_progress);
	}

	if (_sound_asset) {
		_sound_asset->hash (set_progress);
	}
}

Frame
ReelWriter::start () const
{
	return _period.from.frames_floor (_film->video_frame_rate());
}


void
ReelWriter::write (shared_ptr<const AudioBuffers> audio)
{
	if (!_sound_asset_writer) {
		return;
	}

	DCPOMATIC_ASSERT (audio);
	_sound_asset_writer->write (audio->data(), audio->frames());
}

void
ReelWriter::write (PlayerText subs, TextType type, DCPTimePeriod period)
{
	if (!_caption_asset[type]) {
		string lang = _film->subtitle_language ();
		if (lang.empty ()) {
			lang = "Unknown";
		}
		if (_film->interop ()) {
			shared_ptr<dcp::InteropSubtitleAsset> s (new dcp::InteropSubtitleAsset ());
			s->set_movie_title (_film->name ());
			s->set_language (lang);
			s->set_reel_number (raw_convert<string> (_reel_index + 1));
			_caption_asset[type] = s;
		} else {
			shared_ptr<dcp::SMPTESubtitleAsset> s (new dcp::SMPTESubtitleAsset ());
			s->set_content_title_text (_film->name ());
			s->set_language (lang);
			s->set_edit_rate (dcp::Fraction (_film->video_frame_rate (), 1));
			s->set_reel_number (_reel_index + 1);
			s->set_time_code_rate (_film->video_frame_rate ());
			s->set_start_time (dcp::Time ());
			if (_film->encrypted ()) {
				s->set_key (_film->key ());
			}
			_caption_asset[type] = s;
		}
	}

	BOOST_FOREACH (StringText i, subs.text) {
		/* XXX: couldn't / shouldn't we use period here rather than getting time from the subtitle? */
		i.set_in  (i.in()  - dcp::Time (_period.from.seconds(), i.in().tcr));
		i.set_out (i.out() - dcp::Time (_period.from.seconds(), i.out().tcr));
		_caption_asset[type]->add (shared_ptr<dcp::Subtitle>(new dcp::SubtitleString(i)));
	}

	BOOST_FOREACH (BitmapText i, subs.image) {
		_caption_asset[type]->add (
			shared_ptr<dcp::Subtitle>(
				new dcp::SubtitleImage(
					i.image->as_png(),
					dcp::Time(period.from.seconds(), _film->video_frame_rate()),
					dcp::Time(period.to.seconds(), _film->video_frame_rate()),
					i.rectangle.x, dcp::HALIGN_LEFT, i.rectangle.y, dcp::VALIGN_TOP,
					dcp::Time(), dcp::Time()
					)
				)
			);
	}
}

bool
ReelWriter::existing_picture_frame_ok (FILE* asset_file, FILE* info_file, Frame frame) const
{
	LOG_GENERAL ("Checking existing picture frame %1", frame);

	/* Read the data from the info file; for 3D we just check the left
	   frames until we find a good one.
	*/
	dcp::FrameInfo const info = read_frame_info (info_file, frame, _film->three_d () ? EYES_LEFT : EYES_BOTH);

	bool ok = true;

	/* Read the data from the asset and hash it */
	dcpomatic_fseek (asset_file, info.offset, SEEK_SET);
	Data data (info.size);
	size_t const read = fread (data.data().get(), 1, data.size(), asset_file);
	LOG_GENERAL ("Read %1 bytes of asset data; wanted %2", read, info.size);
	if (read != static_cast<size_t> (data.size ())) {
		LOG_GENERAL ("Existing frame %1 is incomplete", frame);
		ok = false;
	} else {
		Digester digester;
		digester.add (data.data().get(), data.size());
		LOG_GENERAL ("Hash %1 vs %2", digester.get(), info.hash);
		if (digester.get() != info.hash) {
			LOG_GENERAL ("Existing frame %1 failed hash check", frame);
			ok = false;
		}
	}

	return ok;
}
