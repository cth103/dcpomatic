/*
    Copyright (C) 2012-2020 Carl Hetherington <cth@carlh.net>

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
#include "dcpomatic_log.h"
#include "digester.h"
#include "font.h"
#include "compose.hpp"
#include "config.h"
#include "audio_buffers.h"
#include "image.h"
#include <dcp/atmos_asset.h>
#include <dcp/atmos_asset_writer.h>
#include <dcp/mono_picture_asset.h>
#include <dcp/stereo_picture_asset.h>
#include <dcp/sound_asset.h>
#include <dcp/sound_asset_writer.h>
#include <dcp/reel.h>
#include <dcp/reel_atmos_asset.h>
#include <dcp/reel_mono_picture_asset.h>
#include <dcp/reel_stereo_picture_asset.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/reel_subtitle_asset.h>
#include <dcp/reel_closed_caption_asset.h>
#include <dcp/reel_markers_asset.h>
#include <dcp/dcp.h>
#include <dcp/cpl.h>
#include <dcp/certificate_chain.h>
#include <dcp/interop_subtitle_asset.h>
#include <dcp/smpte_subtitle_asset.h>
#include <dcp/raw_convert.h>
#include <dcp/subtitle_image.h>
#include <boost/foreach.hpp>

#include "i18n.h"

using std::list;
using std::string;
using std::cout;
using std::exception;
using std::map;
using std::vector;
using boost::shared_ptr;
using boost::optional;
using boost::dynamic_pointer_cast;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using dcp::ArrayData;
using dcp::Data;
using dcp::raw_convert;
using namespace dcpomatic;

int const ReelWriter::_info_size = 48;

static dcp::MXFMetadata
mxf_metadata ()
{
	dcp::MXFMetadata meta;
	Config* config = Config::instance();
	if (!config->dcp_company_name().empty()) {
		meta.company_name = config->dcp_company_name ();
	}
	if (!config->dcp_product_name().empty()) {
		meta.product_name = config->dcp_product_name ();
	}
	if (!config->dcp_product_version().empty()) {
		meta.product_version = config->dcp_product_version ();
	}
	return meta;
}

/** @param job Related job, or 0 */
ReelWriter::ReelWriter (
	shared_ptr<const Film> film, DCPTimePeriod period, shared_ptr<Job> job, int reel_index, int reel_count, optional<string> content_summary
	)
	: _film (film)
	, _period (period)
	, _reel_index (reel_index)
	, _reel_count (reel_count)
	, _content_summary (content_summary)
	, _job (job)
{
	/* Create or find our picture asset in a subdirectory, named
	   according to those film's parameters which affect the video
	   output.  We will hard-link it into the DCP later.
	*/

	dcp::Standard const standard = _film->interop() ? dcp::INTEROP : dcp::SMPTE;

	boost::filesystem::path const asset =
		_film->internal_video_asset_dir() / _film->internal_video_asset_filename(_period);

	_first_nonexistant_frame = check_existing_picture_asset (asset);

	if (_first_nonexistant_frame < period.duration().frames_round(_film->video_frame_rate())) {
		/* We do not have a complete picture asset.  If there is an
		   existing asset, break any hard links to it as we are about
		   to change its contents (if only by changing the IDs); see
		   #1126.
		*/
		if (boost::filesystem::exists(asset) && boost::filesystem::hard_link_count(asset) > 1) {
			if (job) {
				job->sub (_("Copying old video file"));
				copy_in_bits (asset, asset.string() + ".tmp", bind(&Job::set_progress, job.get(), _1, false));
			} else {
				boost::filesystem::copy_file (asset, asset.string() + ".tmp");
			}
			boost::filesystem::remove (asset);
			boost::filesystem::rename (asset.string() + ".tmp", asset);
		}


		if (_film->three_d ()) {
			_picture_asset.reset (new dcp::StereoPictureAsset(dcp::Fraction(_film->video_frame_rate(), 1), standard));
		} else {
			_picture_asset.reset (new dcp::MonoPictureAsset(dcp::Fraction(_film->video_frame_rate(), 1), standard));
		}

		_picture_asset->set_size (_film->frame_size());
		_picture_asset->set_metadata (mxf_metadata());

		if (_film->encrypted ()) {
			_picture_asset->set_key (_film->key());
			_picture_asset->set_context_id (_film->context_id());
		}

		_picture_asset->set_file (asset);
		_picture_asset_writer = _picture_asset->start_write (asset, _first_nonexistant_frame > 0);
	} else {
		/* We already have a complete picture asset that we can just re-use */
		/* XXX: what about if the encryption key changes? */
		if (_film->three_d ()) {
			_picture_asset.reset (new dcp::StereoPictureAsset(asset));
		} else {
			_picture_asset.reset (new dcp::MonoPictureAsset(asset));
		}
	}

	if (_film->audio_channels ()) {
		_sound_asset.reset (
			new dcp::SoundAsset (dcp::Fraction(_film->video_frame_rate(), 1), _film->audio_frame_rate(), _film->audio_channels(), _film->audio_language(), standard)
			);

		_sound_asset->set_metadata (mxf_metadata());

		if (_film->encrypted ()) {
			_sound_asset->set_key (_film->key ());
		}

		DCPOMATIC_ASSERT (_film->directory());

		vector<dcp::Channel> active;
		BOOST_FOREACH (int i, _film->mapped_audio_channels()) {
			active.push_back (static_cast<dcp::Channel>(i));
		}

		/* Write the sound asset into the film directory so that we leave the creation
		   of the DCP directory until the last minute.
		*/
		_sound_asset_writer = _sound_asset->start_write (
			_film->directory().get() / audio_asset_filename (_sound_asset, _reel_index, _reel_count, _content_summary),
			active,
			_film->contains_atmos_content()
			);
	}
}

/** @param frame reel-relative frame */
void
ReelWriter::write_frame_info (Frame frame, Eyes eyes, dcp::FrameInfo info) const
{
	shared_ptr<InfoFileHandle> handle = _film->info_file_handle(_period, false);
	dcpomatic_fseek (handle->get(), frame_info_position(frame, eyes), SEEK_SET);
	checked_fwrite (&info.offset, sizeof(info.offset), handle->get(), handle->file());
	checked_fwrite (&info.size, sizeof (info.size), handle->get(), handle->file());
	checked_fwrite (info.hash.c_str(), info.hash.size(), handle->get(), handle->file());
}

dcp::FrameInfo
ReelWriter::read_frame_info (shared_ptr<InfoFileHandle> info, Frame frame, Eyes eyes) const
{
	dcp::FrameInfo frame_info;
	dcpomatic_fseek (info->get(), frame_info_position(frame, eyes), SEEK_SET);
	checked_fread (&frame_info.offset, sizeof(frame_info.offset), info->get(), info->file());
	checked_fread (&frame_info.size, sizeof(frame_info.size), info->get(), info->file());

	char hash_buffer[33];
	checked_fread (hash_buffer, 32, info->get(), info->file());
	hash_buffer[32] = '\0';
	frame_info.hash = hash_buffer;

	return frame_info;
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
ReelWriter::check_existing_picture_asset (boost::filesystem::path asset)
{
	shared_ptr<Job> job = _job.lock ();

	if (job) {
		job->sub (_("Checking existing image data"));
	}

	/* Try to open the existing asset */
	FILE* asset_file = fopen_boost (asset, "rb");
	if (!asset_file) {
		LOG_GENERAL ("Could not open existing asset at %1 (errno=%2)", asset.string(), errno);
		return 0;
	} else {
		LOG_GENERAL ("Opened existing asset at %1", asset.string());
	}

	shared_ptr<InfoFileHandle> info_file;

	try {
		info_file = _film->info_file_handle (_period, true);
	} catch (OpenFileError &) {
		LOG_GENERAL_NC ("Could not open film info file");
		fclose (asset_file);
		return 0;
	}

	/* Offset of the last dcp::FrameInfo in the info file */
	int const n = (boost::filesystem::file_size(info_file->file()) / _info_size) - 1;
	LOG_GENERAL ("The last FI is %1; info file is %2, info size %3", n, boost::filesystem::file_size(info_file->file()), _info_size);

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

	return first_nonexistant_frame;
}

void
ReelWriter::write (shared_ptr<const Data> encoded, Frame frame, Eyes eyes)
{
	if (!_picture_asset_writer) {
		/* We're not writing any data */
		return;
	}

	dcp::FrameInfo fin = _picture_asset_writer->write (encoded->data(), encoded->size());
	write_frame_info (frame, eyes, fin);
	_last_written[eyes] = encoded;
}


void
ReelWriter::write (shared_ptr<const dcp::AtmosFrame> atmos, AtmosMetadata metadata)
{
	if (!_atmos_asset) {
		_atmos_asset = metadata.create (dcp::Fraction(_film->video_frame_rate(), 1));
		if (_film->encrypted()) {
			_atmos_asset->set_key(_film->key());
		}
		_atmos_asset_writer = _atmos_asset->start_write (
			_film->directory().get() / atmos_asset_filename (_atmos_asset, _reel_index, _reel_count, _content_summary)
			);
	}
	_atmos_asset_writer->write (atmos);
}


void
ReelWriter::fake_write (int size)
{
	if (!_picture_asset_writer) {
		/* We're not writing any data */
		return;
	}

	_picture_asset_writer->fake_write (size);
}

void
ReelWriter::repeat_write (Frame frame, Eyes eyes)
{
	if (!_picture_asset_writer) {
		/* We're not writing any data */
		return;
	}

	dcp::FrameInfo fin = _picture_asset_writer->write (
		_last_written[eyes]->data(),
		_last_written[eyes]->size()
		);
	write_frame_info (frame, eyes, fin);
}

void
ReelWriter::finish ()
{
	if (_picture_asset_writer && !_picture_asset_writer->finalize ()) {
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
		/* There may be an existing "to" file if we are recreating a DCP in the same place without
		   changing any video.
		*/
		boost::system::error_code ec;
		boost::filesystem::remove (video_to, ec);

		boost::filesystem::create_hard_link (video_from, video_to, ec);
		if (ec) {
			LOG_WARNING_NC ("Hard-link failed; copying instead");
			shared_ptr<Job> job = _job.lock ();
			if (job) {
				job->sub (_("Copying video file into DCP"));
				try {
					copy_in_bits (video_from, video_to, bind(&Job::set_progress, job.get(), _1, false));
				} catch (exception& e) {
					LOG_ERROR ("Failed to copy video file from %1 to %2 (%3)", video_from.string(), video_to.string(), e.what());
					throw FileError (e.what(), video_from);
				}
			} else {
				boost::filesystem::copy_file (video_from, video_to, ec);
				if (ec) {
					LOG_ERROR ("Failed to copy video file from %1 to %2 (%3)", video_from.string(), video_to.string(), ec.message());
					throw FileError (ec.message(), video_from);
				}
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

	if (_atmos_asset) {
		_atmos_asset_writer->finalize ();
		boost::filesystem::path atmos_to;
		atmos_to /= _film->dir (_film->dcp_name());
		string const aaf = atmos_asset_filename (_atmos_asset, _reel_index, _reel_count, _content_summary);
		atmos_to /= aaf;

		boost::system::error_code ec;
		boost::filesystem::rename (_film->file(aaf), atmos_to, ec);
		if (ec) {
			throw FileError (
				String::compose (_("could not move atmos asset into the DCP (%1)"), ec.value ()), aaf
				);
		}

		_atmos_asset->set_file (atmos_to);
	}
}

template <class T>
shared_ptr<T>
maybe_add_text (
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
		/* Add the font to the subtitle content */
		BOOST_FOREACH (shared_ptr<Font> j, fonts) {
			asset->add_font (j->id(), j->file().get_value_or(default_font_file()));
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
		if (reel_asset->actual_duration() != period_duration) {
			throw ProgrammingError (
				__FILE__, __LINE__,
				String::compose ("%1 vs %2", reel_asset->actual_duration(), period_duration)
				);
		}
		reel->add (reel_asset);
	}

	return reel_asset;
}

shared_ptr<dcp::Reel>
ReelWriter::create_reel (list<ReferencedReelAsset> const & refs, list<shared_ptr<Font> > const & fonts)
{
	LOG_GENERAL ("create_reel for %1-%2; %3 of %4", _period.from.get(), _period.to.get(), _reel_index, _reel_count);

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

	Frame const period_duration = _period.duration().frames_round(_film->video_frame_rate());

	DCPOMATIC_ASSERT (reel_picture_asset);
	if (reel_picture_asset->duration() != period_duration) {
		throw ProgrammingError (
			__FILE__, __LINE__,
			String::compose ("%1 vs %2", reel_picture_asset->actual_duration(), period_duration)
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
		LOG_GENERAL ("no sound asset of our own; look through %1", refs.size());
		/* We don't have a sound asset of our own; hopefully we have one to reference */
		BOOST_FOREACH (ReferencedReelAsset j, refs) {
			shared_ptr<dcp::ReelSoundAsset> k = dynamic_pointer_cast<dcp::ReelSoundAsset> (j.asset);
			if (k) {
				LOG_GENERAL ("candidate sound asset period is %1-%2", j.period.from.get(), j.period.to.get());
			}
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
	if (reel_sound_asset->actual_duration() != period_duration) {
		LOG_ERROR (
			"Reel sound asset has length %1 but reel period is %2",
			reel_sound_asset->actual_duration(),
			period_duration
			);
		if (reel_sound_asset->actual_duration() != period_duration) {
			throw ProgrammingError (
				__FILE__, __LINE__,
				String::compose ("%1 vs %2", reel_sound_asset->actual_duration(), period_duration)
				);
		}

	}
	reel->add (reel_sound_asset);

	shared_ptr<dcp::ReelSubtitleAsset> subtitle = maybe_add_text<dcp::ReelSubtitleAsset> (_subtitle_asset, reel_picture_asset->actual_duration(), reel, refs, fonts, _film, _period);
	if (subtitle && !_film->subtitle_languages().empty()) {
		subtitle->set_language (_film->subtitle_languages().front());
	}

	for (map<DCPTextTrack, shared_ptr<dcp::SubtitleAsset> >::const_iterator i = _closed_caption_assets.begin(); i != _closed_caption_assets.end(); ++i) {
		shared_ptr<dcp::ReelClosedCaptionAsset> a = maybe_add_text<dcp::ReelClosedCaptionAsset> (
			i->second, reel_picture_asset->actual_duration(), reel, refs, fonts, _film, _period
			);
		if (a) {
			a->set_annotation_text (i->first.name);
			a->set_language (dcp::LanguageTag(i->first.language));
		}
	}

	Film::Markers markers = _film->markers ();
	_film->add_ffoc_lfoc (markers);
	Film::Markers reel_markers;
	for (Film::Markers::const_iterator i = markers.begin(); i != markers.end(); ++i) {
		if (_period.contains(i->second)) {
			reel_markers[i->first] = i->second;
		}
	}

	if (!reel_markers.empty ()) {
		shared_ptr<dcp::ReelMarkersAsset> ma (new dcp::ReelMarkersAsset(dcp::Fraction(_film->video_frame_rate(), 1), 0));
		for (map<dcp::Marker, DCPTime>::const_iterator i = reel_markers.begin(); i != reel_markers.end(); ++i) {
			int h, m, s, f;
			DCPTime relative = i->second - _period.from;
			relative.split (_film->video_frame_rate(), h, m, s, f);
			ma->set (i->first, dcp::Time(h, m, s, f, _film->video_frame_rate()));
		}
		reel->add (ma);
	}

	if (_atmos_asset) {
		reel->add (shared_ptr<dcp::ReelAtmosAsset>(new dcp::ReelAtmosAsset(_atmos_asset, 0)));
	}

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

	if (_atmos_asset) {
		_atmos_asset->hash (set_progress);
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
ReelWriter::write (PlayerText subs, TextType type, optional<DCPTextTrack> track, DCPTimePeriod period)
{
	shared_ptr<dcp::SubtitleAsset> asset;

	switch (type) {
	case TEXT_OPEN_SUBTITLE:
		asset = _subtitle_asset;
		break;
	case TEXT_CLOSED_CAPTION:
		DCPOMATIC_ASSERT (track);
		asset = _closed_caption_assets[*track];
		break;
	default:
		DCPOMATIC_ASSERT (false);
	}

	if (!asset) {
		vector<dcp::LanguageTag> lang = _film->subtitle_languages ();
		if (_film->interop ()) {
			shared_ptr<dcp::InteropSubtitleAsset> s (new dcp::InteropSubtitleAsset ());
			s->set_movie_title (_film->name ());
			if (type == TEXT_OPEN_SUBTITLE) {
				s->set_language (lang.empty() ? "Unknown" : lang.front().to_string());
			} else {
				s->set_language (track->language);
			}
			s->set_reel_number (raw_convert<string> (_reel_index + 1));
			asset = s;
		} else {
			shared_ptr<dcp::SMPTESubtitleAsset> s (new dcp::SMPTESubtitleAsset ());
			s->set_content_title_text (_film->name ());
			s->set_metadata (mxf_metadata());
			if (type == TEXT_OPEN_SUBTITLE && !lang.empty()) {
				s->set_language (lang.front());
			} else {
				s->set_language (dcp::LanguageTag(track->language));
			}
			s->set_edit_rate (dcp::Fraction (_film->video_frame_rate (), 1));
			s->set_reel_number (_reel_index + 1);
			s->set_time_code_rate (_film->video_frame_rate ());
			s->set_start_time (dcp::Time ());
			if (_film->encrypted ()) {
				s->set_key (_film->key ());
			}
			asset = s;
		}
	}

	switch (type) {
	case TEXT_OPEN_SUBTITLE:
		_subtitle_asset = asset;
		break;
	case TEXT_CLOSED_CAPTION:
		DCPOMATIC_ASSERT (track);
		_closed_caption_assets[*track] = asset;
		break;
	default:
		DCPOMATIC_ASSERT (false);
	}

	BOOST_FOREACH (StringText i, subs.string) {
		/* XXX: couldn't / shouldn't we use period here rather than getting time from the subtitle? */
		i.set_in  (i.in()  - dcp::Time (_period.from.seconds(), i.in().tcr));
		i.set_out (i.out() - dcp::Time (_period.from.seconds(), i.out().tcr));
		asset->add (shared_ptr<dcp::Subtitle>(new dcp::SubtitleString(i)));
	}

	BOOST_FOREACH (BitmapText i, subs.bitmap) {
		asset->add (
			shared_ptr<dcp::Subtitle>(
				new dcp::SubtitleImage(
					i.image->as_png(),
					dcp::Time(period.from.seconds() - _period.from.seconds(), _film->video_frame_rate()),
					dcp::Time(period.to.seconds() - _period.from.seconds(), _film->video_frame_rate()),
					i.rectangle.x, dcp::HALIGN_LEFT, i.rectangle.y, dcp::VALIGN_TOP,
					dcp::Time(), dcp::Time()
					)
				)
			);
	}
}

bool
ReelWriter::existing_picture_frame_ok (FILE* asset_file, shared_ptr<InfoFileHandle> info_file, Frame frame) const
{
	LOG_GENERAL ("Checking existing picture frame %1", frame);

	/* Read the data from the info file; for 3D we just check the left
	   frames until we find a good one.
	*/
	dcp::FrameInfo const info = read_frame_info (info_file, frame, _film->three_d () ? EYES_LEFT : EYES_BOTH);

	bool ok = true;

	/* Read the data from the asset and hash it */
	dcpomatic_fseek (asset_file, info.offset, SEEK_SET);
	ArrayData data (info.size);
	size_t const read = fread (data.data(), 1, data.size(), asset_file);
	LOG_GENERAL ("Read %1 bytes of asset data; wanted %2", read, info.size);
	if (read != static_cast<size_t> (data.size ())) {
		LOG_GENERAL ("Existing frame %1 is incomplete", frame);
		ok = false;
	} else {
		Digester digester;
		digester.add (data.data(), data.size());
		LOG_GENERAL ("Hash %1 vs %2", digester.get(), info.hash);
		if (digester.get() != info.hash) {
			LOG_GENERAL ("Existing frame %1 failed hash check", frame);
			ok = false;
		}
	}

	return ok;
}
