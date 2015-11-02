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

#include "reel_writer.h"
#include "film.h"
#include "cross.h"
#include "job.h"
#include "log.h"
#include "md5_digester.h"
#include "font.h"
#include "compose.hpp"
#include "audio_buffers.h"
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

#include "i18n.h"

#define LOG_GENERAL(...) _film->log()->log (String::compose (__VA_ARGS__), LogEntry::TYPE_GENERAL);
#define LOG_GENERAL_NC(...) _film->log()->log (__VA_ARGS__, LogEntry::TYPE_GENERAL);
#define LOG_WARNING_NC(...) _film->log()->log (__VA_ARGS__, LogEntry::TYPE_WARNING);
#define LOG_ERROR(...) _film->log()->log (String::compose (__VA_ARGS__), LogEntry::TYPE_ERROR);

using std::list;
using std::string;
using boost::shared_ptr;
using boost::optional;
using boost::dynamic_pointer_cast;

int const ReelWriter::_info_size = 48;

ReelWriter::ReelWriter (shared_ptr<const Film> film, DCPTimePeriod period, shared_ptr<Job> job)
	: _film (film)
	, _period (period)
	, _first_nonexistant_frame (0)
	, _last_written_video_frame (-1)
	, _last_written_eyes (EYES_RIGHT)
	, _total_written_audio_frames (0)
{
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
		_film->internal_video_asset_dir() / _film->internal_video_asset_filename(_period)
		);

	job->sub (_("Checking existing image data"));
	check_existing_picture_asset ();

	_picture_asset_writer = _picture_asset->start_write (
		_film->internal_video_asset_dir() / _film->internal_video_asset_filename(_period),
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
}

/** @param frame reel-relative frame */
void
ReelWriter::write_frame_info (Frame frame, Eyes eyes, dcp::FrameInfo info) const
{
	FILE* file = 0;
	boost::filesystem::path info_file = _film->info_file (_period);
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

void
ReelWriter::check_existing_picture_asset ()
{
	/* Try to open the existing asset */
	FILE* asset_file = fopen_boost (_picture_asset->file(), "rb");
	if (!asset_file) {
		LOG_GENERAL ("Could not open existing asset at %1 (errno=%2)", _picture_asset->file().string(), errno);
		return;
	}

	/* Offset of the last dcp::FrameInfo in the info file */
	int const n = (boost::filesystem::file_size (_film->info_file(_period)) / _info_size) - 1;

	FILE* info_file = fopen_boost (_film->info_file(_period), "rb");
	if (!info_file) {
		LOG_GENERAL_NC ("Could not open film info file");
		fclose (asset_file);
		return;
	}

	if (_film->three_d ()) {
		/* Start looking at the last left frame */
		_first_nonexistant_frame = n / 2;
	} else {
		_first_nonexistant_frame = n;
	}

	bool ok = false;

	while (!ok) {
		/* Read the data from the info file; for 3D we just check the left
		   frames until we find a good one.
		*/
		dcp::FrameInfo info = read_frame_info (info_file, _first_nonexistant_frame, _film->three_d () ? EYES_LEFT : EYES_BOTH);

		ok = true;

		/* Read the data from the asset and hash it */
		dcpomatic_fseek (asset_file, info.offset, SEEK_SET);
		Data data (info.size);
		size_t const read = fread (data.data().get(), 1, data.size(), asset_file);
		if (read != static_cast<size_t> (data.size ())) {
			LOG_GENERAL ("Existing frame %1 is incomplete", _first_nonexistant_frame);
			ok = false;
		} else {
			MD5Digester digester;
			digester.add (data.data().get(), data.size());
			if (digester.get() != info.hash) {
				LOG_GENERAL ("Existing frame %1 failed hash check", _first_nonexistant_frame);
				ok = false;
			}
		}

		if (!ok) {
			--_first_nonexistant_frame;
		}
	}

	if (!_film->three_d ()) {
		/* If we are doing 3D we might have found a good L frame with no R, so only
		   do this if we're in 2D and we've just found a good B(oth) frame.
		*/
		++_first_nonexistant_frame;
	}

	fclose (asset_file);
	fclose (info_file);
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
		_picture_asset.reset ();
	}

	if (_sound_asset_writer && !_sound_asset_writer->finalize ()) {
		/* Nothing was written to the sound asset */
		_sound_asset.reset ();
	}

	/* Hard-link any video asset file into the DCP */
	if (_picture_asset) {
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
	}

	/* Move the audio asset into the DCP */
	if (_sound_asset) {
		boost::filesystem::path audio_to;
		audio_to /= _film->dir (_film->dcp_name ());
		audio_to /= audio_asset_filename (_sound_asset);

		boost::system::error_code ec;
		boost::filesystem::rename (_film->file (audio_asset_filename (_sound_asset)), audio_to, ec);
		if (ec) {
			throw FileError (
				String::compose (_("could not move audio asset into the DCP (%1)"), ec.value ()), audio_asset_filename (_sound_asset)
				);
		}

		_sound_asset->set_file (audio_to);
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
		/* We don't have a picture asset of our own; hopefully we have one to reference */
		BOOST_FOREACH (ReferencedReelAsset j, refs) {
			shared_ptr<dcp::ReelPictureAsset> k = dynamic_pointer_cast<dcp::ReelPictureAsset> (j.asset);
			if (k && j.period == _period) {
				reel_picture_asset = k;
			}
		}
	}

	reel->add (reel_picture_asset);

	if (_sound_asset) {
		/* We have made a sound asset of our own.  Put it into the reel */
		reel->add (shared_ptr<dcp::ReelSoundAsset> (new dcp::ReelSoundAsset (_sound_asset, 0)));
	} else {
		/* We don't have a sound asset of our own; hopefully we have one to reference */
		BOOST_FOREACH (ReferencedReelAsset j, refs) {
			shared_ptr<dcp::ReelSoundAsset> k = dynamic_pointer_cast<dcp::ReelSoundAsset> (j.asset);
			if (k && j.period == _period) {
				reel->add (k);
			}
		}
	}

	if (_subtitle_asset) {

		boost::filesystem::path liberation_normal;
		try {
			liberation_normal = shared_path () / "LiberationSans-Regular.ttf";
		} catch (boost::filesystem::filesystem_error& e) {
			/* Hack: try the debian/ubuntu location if getting the shared path failed */
			liberation_normal = "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf";
		}


		/* Add all the fonts to the subtitle content */
		BOOST_FOREACH (shared_ptr<Font> j, fonts) {
			_subtitle_asset->add_font (j->id(), j->file(FontFiles::NORMAL).get_value_or(liberation_normal));
		}

		if (dynamic_pointer_cast<dcp::InteropSubtitleAsset> (_subtitle_asset)) {
			boost::filesystem::path directory = _film->dir (_film->dcp_name ()) / _subtitle_asset->id ();
			boost::filesystem::create_directories (directory);
			_subtitle_asset->write (directory / ("sub_" + _subtitle_asset->id() + ".xml"));
		} else {

			/* All our assets should be the same length; use the picture asset length here
			   as a reference to set the subtitle one.
			*/
			dynamic_pointer_cast<dcp::SMPTESubtitleAsset>(_subtitle_asset)->set_intrinsic_duration (
				reel_picture_asset->intrinsic_duration ()
				);

			_subtitle_asset->write (
				_film->dir (_film->dcp_name ()) / ("sub_" + _subtitle_asset->id() + ".mxf")
				);
		}

		reel->add (shared_ptr<dcp::ReelSubtitleAsset> (
				   new dcp::ReelSubtitleAsset (
					   _subtitle_asset,
					   dcp::Fraction (_film->video_frame_rate(), 1),
					   reel_picture_asset->intrinsic_duration (),
					   0
					   )
				   ));
	} else {
		/* We don't have a subtitle asset of our own; hopefully we have one to reference */
		BOOST_FOREACH (ReferencedReelAsset j, refs) {
			shared_ptr<dcp::ReelSubtitleAsset> k = dynamic_pointer_cast<dcp::ReelSubtitleAsset> (j.asset);
			if (k && j.period == _period) {
				reel->add (k);
			}
		}
	}

	return reel;
}

void
ReelWriter::calculate_digests (shared_ptr<Job> job)
{
	job->sub (_("Computing image digest"));
	if (_picture_asset) {
		_picture_asset->hash (boost::bind (&Job::set_progress, job.get(), _1, false));
	}

	if (_sound_asset) {
		job->sub (_("Computing audio digest"));
		_sound_asset->hash (boost::bind (&Job::set_progress, job.get(), _1, false));
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

	if (audio) {
		_sound_asset_writer->write (audio->data(), audio->frames());
	}

	++_total_written_audio_frames;
}

void
ReelWriter::write (PlayerSubtitles subs)
{
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
			s->set_reel_number (1);
			s->set_time_code_rate (_film->video_frame_rate ());
			s->set_start_time (dcp::Time ());
			_subtitle_asset = s;
		}
	}

	for (list<dcp::SubtitleString>::const_iterator i = subs.text.begin(); i != subs.text.end(); ++i) {
		_subtitle_asset->add (*i);
	}
}
