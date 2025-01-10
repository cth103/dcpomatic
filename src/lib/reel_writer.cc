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


#include "audio_buffers.h"
#include "compose.hpp"
#include "config.h"
#include "constants.h"
#include "cross.h"
#include "dcpomatic_log.h"
#include "digester.h"
#include "film.h"
#include "film_util.h"
#include "frame_info.h"
#include "image.h"
#include "image_png.h"
#include "job.h"
#include "log.h"
#include "reel_writer.h"
#include "remembered_asset.h"
#include <dcp/atmos_asset.h>
#include <dcp/atmos_asset_writer.h>
#include <dcp/certificate_chain.h>
#include <dcp/cpl.h>
#include <dcp/dcp.h>
#include <dcp/filesystem.h>
#include <dcp/interop_text_asset.h>
#include <dcp/mono_j2k_picture_asset.h>
#include <dcp/reel.h>
#include <dcp/reel_atmos_asset.h>
#include <dcp/reel_interop_text_asset.h>
#include <dcp/reel_markers_asset.h>
#include <dcp/reel_mono_picture_asset.h>
#include <dcp/reel_smpte_text_asset.h>
#include <dcp/reel_sound_asset.h>
#include <dcp/reel_stereo_picture_asset.h>
#include <dcp/smpte_text_asset.h>
#include <dcp/sound_asset.h>
#include <dcp/sound_asset_writer.h>
#include <dcp/stereo_j2k_picture_asset.h>
#include <dcp/text_image.h>
#include <fmt/format.h>

#include "i18n.h"


using std::dynamic_pointer_cast;
using std::exception;
using std::list;
using std::make_shared;
using std::map;
using std::set;
using std::shared_ptr;
using std::string;
using std::vector;
using std::weak_ptr;
using boost::optional;
#if BOOST_VERSION >= 106100
using namespace boost::placeholders;
#endif
using dcp::ArrayData;
using dcp::Data;
using namespace dcpomatic;


static dcp::MXFMetadata
mxf_metadata ()
{
	dcp::MXFMetadata meta;
	auto config = Config::instance();
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


/** @param job Related job, or 0.
 *  @param text_only true to enable a special mode where the writer will expect only subtitles and closed captions to be written
 *  (no picture nor sound) and not give errors in that case.  This is used by the hints system to check the potential sizes of
 *  subtitle / closed caption files.
 */
ReelWriter::ReelWriter (
	weak_ptr<const Film> weak_film, DCPTimePeriod period, shared_ptr<Job> job, int reel_index, int reel_count, bool text_only, boost::filesystem::path output_dir
	)
	: WeakConstFilm(weak_film)
	, _output_dir(std::move(output_dir))
	, _period (period)
	, _reel_index (reel_index)
	, _reel_count (reel_count)
	, _content_summary (film()->content_summary(period))
	, _job (job)
	, _text_only (text_only)
	, _info_file(film()->info_file(period), dcp::filesystem::exists(film()->info_file(period)) ? "r+b" : "wb")
	, _font_metrics(film()->frame_size().height)
{
	if (!_info_file) {
		auto const info_file_path = film()->info_file(period);
		throw OpenFileError(info_file_path, _info_file.open_error(), dcp::filesystem::exists(info_file_path) ? OpenFileError::READ_WRITE : OpenFileError::WRITE);
	}

	_default_font = dcp::ArrayData(default_font_file());

	if (text_only) {
		return;
	}

	auto const standard = film()->interop() ? dcp::Standard::INTEROP : dcp::Standard::SMPTE;

	auto remembered_assets = film()->read_remembered_assets();
	DCPOMATIC_ASSERT(film()->directory());

	auto existing_asset_filename = find_asset(remembered_assets, *film()->directory(), period, film()->video_identifier());
	if (existing_asset_filename) {
		_first_nonexistent_frame = check_existing_picture_asset(*existing_asset_filename);
	}

	if (_first_nonexistent_frame < period.duration().frames_round(film()->video_frame_rate())) {
		/* No existing asset, or an incomplete one */

		auto const rate = dcp::Fraction(film()->video_frame_rate(), 1);

		auto setup = [this](shared_ptr<dcp::PictureAsset> asset) {
			asset->set_size(film()->frame_size());
			asset->set_metadata(mxf_metadata());

			if (film()->encrypted()) {
				asset->set_key(film()->key());
				asset->set_context_id(film()->context_id());
			}
		};

		shared_ptr<dcp::PictureAsset> picture_asset;

		if (film()->video_encoding() == VideoEncoding::JPEG2000) {
			if (film()->three_d()) {
				_j2k_picture_asset = std::make_shared<dcp::StereoJ2KPictureAsset>(rate, standard);
			} else {
				_j2k_picture_asset = std::make_shared<dcp::MonoJ2KPictureAsset>(rate, standard);
			}
			setup(_j2k_picture_asset);
			picture_asset = _j2k_picture_asset;
		} else {
			_mpeg2_picture_asset = std::make_shared<dcp::MonoMPEG2PictureAsset>(rate);
			setup(_mpeg2_picture_asset);
			picture_asset = _mpeg2_picture_asset;
		}

		auto new_asset_filename = _output_dir / video_asset_filename(picture_asset, _reel_index, _reel_count, _content_summary);
		if (_first_nonexistent_frame > 0) {
			LOG_GENERAL("Re-using partial asset %1: has frames up to %2", *existing_asset_filename, _first_nonexistent_frame);
			dcp::filesystem::rename(*existing_asset_filename, new_asset_filename);
		}
		remembered_assets.push_back(RememberedAsset(new_asset_filename.filename(), period, film()->video_identifier()));
		film()->write_remembered_assets(remembered_assets);
		picture_asset->set_file(new_asset_filename);

		dcp::Behaviour const behaviour = _first_nonexistent_frame > 0 ? dcp::Behaviour::OVERWRITE_EXISTING : dcp::Behaviour::MAKE_NEW;
		if (_j2k_picture_asset) {
			_j2k_picture_asset_writer = _j2k_picture_asset->start_write(new_asset_filename, behaviour);
		} else {
			_mpeg2_picture_asset_writer = _mpeg2_picture_asset->start_write(new_asset_filename, behaviour);
		}
	} else {
		LOG_GENERAL("Re-using complete asset %1", *existing_asset_filename);
		/* We already have a complete picture asset that we can just re-use */
		/* XXX: what about if the encryption key changes? */
		auto new_asset_filename = _output_dir / existing_asset_filename->filename();
		if (new_asset_filename != *existing_asset_filename) {
			dcp::filesystem::copy(*existing_asset_filename, new_asset_filename);
			remembered_assets.push_back(RememberedAsset(new_asset_filename, period, film()->video_identifier()));
		}
		film()->write_remembered_assets(remembered_assets);

		if (film()->video_encoding() == VideoEncoding::JPEG2000) {
			if (film()->three_d()) {
				_j2k_picture_asset = make_shared<dcp::StereoJ2KPictureAsset>(new_asset_filename);
			} else {
				_j2k_picture_asset = make_shared<dcp::MonoJ2KPictureAsset>(new_asset_filename);
			}
		} else {
			_mpeg2_picture_asset = make_shared<dcp::MonoMPEG2PictureAsset>(new_asset_filename);
		}
	}

	if (film()->audio_channels()) {
		auto lang = film()->audio_language();
		_sound_asset = make_shared<dcp::SoundAsset> (
			dcp::Fraction(film()->video_frame_rate(), 1),
			film()->audio_frame_rate(),
			film()->audio_channels(),
			lang ? *lang : dcp::LanguageTag("en-US"),
			standard
			);

		_sound_asset->set_metadata (mxf_metadata());

		if (film()->encrypted()) {
			_sound_asset->set_key (film()->key());
		}

		DCPOMATIC_ASSERT (film()->directory());

		std::vector<dcp::Channel> extra_active_channels;
		for (auto channel: std::vector<dcp::Channel>{dcp::Channel::HI, dcp::Channel::VI, dcp::Channel::BSL, dcp::Channel::BSR}) {
			if (channel_is_mapped(film(), channel)) {
				extra_active_channels.push_back(channel);
			}
		}

		/* Write the sound asset into the film directory so that we leave the creation
		   of the DCP directory until the last minute.
		*/
		_sound_asset_writer = _sound_asset->start_write (
			film()->directory().get() / audio_asset_filename (_sound_asset, _reel_index, _reel_count, _content_summary),
			extra_active_channels,
			film()->contains_atmos_content() ? dcp::SoundAsset::AtmosSync::ENABLED : dcp::SoundAsset::AtmosSync::DISABLED,
			film()->limit_to_smpte_bv20() ? dcp::SoundAsset::MCASubDescriptors::DISABLED : dcp::SoundAsset::MCASubDescriptors::ENABLED
			);
	}
}


Frame
ReelWriter::check_existing_picture_asset (boost::filesystem::path asset)
{
	auto job = _job.lock ();

	if (job) {
		job->sub (_("Checking existing image data"));
	}

	/* Try to open the existing asset */
	dcp::File asset_file(asset, "rb");
	if (!asset_file) {
		LOG_GENERAL ("Could not open existing asset at %1 (errno=%2)", asset.string(), errno);
		return 0;
	} else {
		LOG_GENERAL ("Opened existing asset at %1", asset.string());
	}

	/* Offset of the last dcp::FrameInfo in the info file */
	int const n = (dcp::filesystem::file_size(_info_file.path()) / J2KFrameInfo::size_on_disk()) - 1;
	LOG_GENERAL("The last FI is %1; info file is %2, info size %3", n, dcp::filesystem::file_size(_info_file.path()), J2KFrameInfo::size_on_disk())

	Frame first_nonexistent_frame;
	if (film()->three_d()) {
		/* Start looking at the last left frame */
		first_nonexistent_frame = n / 2;
	} else {
		first_nonexistent_frame = n;
	}

	while (!existing_picture_frame_ok(asset_file, first_nonexistent_frame) && first_nonexistent_frame > 0) {
		--first_nonexistent_frame;
	}

	if (!film()->three_d() && first_nonexistent_frame > 0) {
		/* If we are doing 3D we might have found a good L frame with no R, so only
		   do this if we're in 2D and we've just found a good B(oth) frame.
		*/
		++first_nonexistent_frame;
	}

	LOG_GENERAL ("Proceeding with first nonexistent frame %1", first_nonexistent_frame);

	return first_nonexistent_frame;
}


void
ReelWriter::write (shared_ptr<const Data> encoded, Frame frame, Eyes eyes)
{
	if (!_j2k_picture_asset_writer) {
		/* We're not writing any data */
		return;
	}

	auto fin = J2KFrameInfo(_j2k_picture_asset_writer->write(encoded->data(), encoded->size()));
	fin.write(_info_file, frame, eyes);
	_last_written[eyes] = encoded;
}


void
ReelWriter::write (shared_ptr<const dcp::AtmosFrame> atmos, AtmosMetadata metadata)
{
	if (!_atmos_asset) {
		_atmos_asset = metadata.create (dcp::Fraction(film()->video_frame_rate(), 1));
		if (film()->encrypted()) {
			_atmos_asset->set_key(film()->key());
		}
		_atmos_asset_writer = _atmos_asset->start_write (
			film()->directory().get() / atmos_asset_filename (_atmos_asset, _reel_index, _reel_count, _content_summary)
			);
	}
	_atmos_asset_writer->write (atmos);
}


void
ReelWriter::write(shared_ptr<dcp::MonoMPEG2PictureFrame> image)
{
	_mpeg2_picture_asset_writer->write(image->data(), image->size());
}


void
ReelWriter::fake_write(Frame frame, Eyes eyes)
{
	if (!_j2k_picture_asset_writer) {
		/* We're not writing any data */
		return;
	}

	_j2k_picture_asset_writer->fake_write(J2KFrameInfo(_info_file, frame, eyes));
}


void
ReelWriter::repeat_write (Frame frame, Eyes eyes)
{
	if (!_j2k_picture_asset_writer) {
		/* We're not writing any data */
		return;
	}

	auto fin = J2KFrameInfo(_j2k_picture_asset_writer->write(_last_written[eyes]->data(), _last_written[eyes]->size()));
	fin.write(_info_file, frame, eyes);
}


void
ReelWriter::finish (boost::filesystem::path output_dcp)
{
	if (_j2k_picture_asset_writer && !_j2k_picture_asset_writer->finalize()) {
		/* Nothing was written to the J2K picture asset */
		LOG_GENERAL("Nothing was written to J2K asset for reel %1 of %2", _reel_index, _reel_count);
		_j2k_picture_asset.reset();
	}

	if (_mpeg2_picture_asset_writer && !_mpeg2_picture_asset_writer->finalize()) {
		/* Nothing was written to the MPEG2 picture asset */
		LOG_GENERAL("Nothing was written to MPEG2 asset for reel %1 of %2", _reel_index, _reel_count);
		_mpeg2_picture_asset.reset();
	}

	if (_sound_asset_writer && !_sound_asset_writer->finalize ()) {
		/* Nothing was written to the sound asset */
		_sound_asset.reset ();
	}

	/* Move the audio asset into the DCP */
	if (_sound_asset) {
		boost::filesystem::path audio_to = output_dcp;
		auto const aaf = audio_asset_filename (_sound_asset, _reel_index, _reel_count, _content_summary);
		audio_to /= aaf;

		boost::system::error_code ec;
		dcp::filesystem::rename(film()->file(aaf), audio_to, ec);
		if (ec) {
			throw FileError (
				String::compose(_("could not move audio asset into the DCP (%1)"), error_details(ec)), aaf
				);
		}

		_sound_asset->set_file (audio_to);
	}

	if (_atmos_asset) {
		_atmos_asset_writer->finalize ();
		boost::filesystem::path atmos_to = output_dcp;
		auto const aaf = atmos_asset_filename (_atmos_asset, _reel_index, _reel_count, _content_summary);
		atmos_to /= aaf;

		boost::system::error_code ec;
		dcp::filesystem::rename(film()->file(aaf), atmos_to, ec);
		if (ec) {
			throw FileError (
				String::compose(_("could not move atmos asset into the DCP (%1)"), error_details(ec)), aaf
				);
		}

		_atmos_asset->set_file (atmos_to);
	}
}


/** Try to make a ReelAsset for a subtitles or closed captions in a given period in the DCP.
 *  A TextAsset can be provided, or we will use one from @ref refs if not.
 */
template <class Interop, class SMPTE, class Result>
shared_ptr<Result>
maybe_add_text (
	shared_ptr<dcp::TextAsset> asset,
	dcp::TextType type,
	int64_t picture_duration,
	shared_ptr<dcp::Reel> reel,
	int reel_index,
	int reel_count,
	optional<string> content_summary,
	list<ReferencedReelAsset> const & refs,
	shared_ptr<const Film> film,
	DCPTimePeriod period,
	boost::filesystem::path output_dcp,
	bool text_only
	)
{
	Frame const period_duration = period.duration().frames_round(film->video_frame_rate());

	shared_ptr<Result> reel_asset;

	if (asset) {
		if (auto interop = dynamic_pointer_cast<dcp::InteropTextAsset>(asset)) {
			auto directory = output_dcp / interop->id ();
			dcp::filesystem::create_directories(directory);
			interop->write (directory / subtitle_asset_filename(asset, reel_index, reel_count, content_summary, ".xml"));
			reel_asset = make_shared<Interop> (
				type,
				interop,
				dcp::Fraction(film->video_frame_rate(), 1),
				picture_duration,
				0
				);
		} else if (auto smpte = dynamic_pointer_cast<dcp::SMPTETextAsset>(asset)) {
			/* All our assets should be the same length; use the picture asset length here
			   as a reference to set the subtitle one.  We'll use the duration rather than
			   the intrinsic duration; we don't care if the picture asset has been trimmed, we're
			   just interested in its presentation length.
			*/
			smpte->set_intrinsic_duration(picture_duration);
			smpte->write (
				output_dcp / subtitle_asset_filename(asset, reel_index, reel_count, content_summary, ".mxf")
				);
			reel_asset = make_shared<SMPTE> (
				type,
				smpte,
				dcp::Fraction(film->video_frame_rate(), 1),
				picture_duration,
				0
				);
		}

	} else {
		/* We don't have a subtitle asset of our own; hopefully we have one to reference */
		for (auto j: refs) {
			auto k = dynamic_pointer_cast<Result> (j.asset);
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
		if (!text_only && reel_asset->actual_duration() != period_duration) {
			throw ProgrammingError (
				__FILE__, __LINE__,
				String::compose ("%1 vs %2", reel_asset->actual_duration(), period_duration)
				);
		}
		reel->add (reel_asset);
	}

	return reel_asset;
}


shared_ptr<dcp::ReelPictureAsset>
ReelWriter::create_reel_picture (shared_ptr<dcp::Reel> reel, list<ReferencedReelAsset> const & refs) const
{
	shared_ptr<dcp::ReelPictureAsset> reel_asset;

	if (_j2k_picture_asset) {
		/* We have made a picture asset of our own.  Put it into the reel */
		if (auto mono = dynamic_pointer_cast<dcp::MonoJ2KPictureAsset>(_j2k_picture_asset)) {
			reel_asset = make_shared<dcp::ReelMonoPictureAsset>(mono, 0);
		}

		if (auto stereo = dynamic_pointer_cast<dcp::StereoJ2KPictureAsset>(_j2k_picture_asset)) {
			reel_asset = make_shared<dcp::ReelStereoPictureAsset>(stereo, 0);
		}
	} else if (_mpeg2_picture_asset) {
		reel_asset = make_shared<dcp::ReelMonoPictureAsset>(_mpeg2_picture_asset, 0);
	} else {
		LOG_GENERAL ("no picture asset of our own; look through %1", refs.size());
		/* We don't have a picture asset of our own; hopefully we have one to reference */
		for (auto j: refs) {
			auto k = dynamic_pointer_cast<dcp::ReelPictureAsset> (j.asset);
			if (k) {
				LOG_GENERAL ("candidate picture asset period is %1-%2", j.period.from.get(), j.period.to.get());
			}
			if (k && j.period == _period) {
				reel_asset = k;
			}
		}
	}

	Frame const period_duration = _period.duration().frames_round(film()->video_frame_rate());

	DCPOMATIC_ASSERT (reel_asset);
	if (reel_asset->duration() != period_duration) {
		throw ProgrammingError (
			__FILE__, __LINE__,
			String::compose ("%1 vs %2", reel_asset->actual_duration(), period_duration)
			);
	}
	reel->add (reel_asset);

	/* If we have a hash for this asset in the CPL, assume that it is correct */
	if (reel_asset->hash()) {
		reel_asset->asset_ref()->set_hash (reel_asset->hash().get());
	}

	return reel_asset;
}


void
ReelWriter::create_reel_sound (shared_ptr<dcp::Reel> reel, list<ReferencedReelAsset> const & refs) const
{
	shared_ptr<dcp::ReelSoundAsset> reel_asset;

	if (_sound_asset) {
		/* We have made a sound asset of our own.  Put it into the reel */
		reel_asset = make_shared<dcp::ReelSoundAsset>(_sound_asset, 0);
	} else {
		LOG_GENERAL ("no sound asset of our own; look through %1", refs.size());
		/* We don't have a sound asset of our own; hopefully we have one to reference */
		for (auto j: refs) {
			auto k = dynamic_pointer_cast<dcp::ReelSoundAsset> (j.asset);
			if (k) {
				LOG_GENERAL ("candidate sound asset period is %1-%2", j.period.from.get(), j.period.to.get());
			}
			if (k && j.period == _period) {
				reel_asset = k;
				/* If we have a hash for this asset in the CPL, assume that it is correct */
				if (k->hash()) {
					k->asset_ref()->set_hash (k->hash().get());
				}
			}
		}
	}

	auto const period_duration = _period.duration().frames_round(film()->video_frame_rate());

	DCPOMATIC_ASSERT (reel_asset);
	if (reel_asset->actual_duration() != period_duration) {
		LOG_ERROR (
			"Reel sound asset has length %1 but reel period is %2",
			reel_asset->actual_duration(),
			period_duration
			);
		if (reel_asset->actual_duration() != period_duration) {
			throw ProgrammingError (
				__FILE__, __LINE__,
				String::compose ("%1 vs %2", reel_asset->actual_duration(), period_duration)
				);
		}

	}
	reel->add (reel_asset);
}


void
ReelWriter::create_reel_text (
	shared_ptr<dcp::Reel> reel,
	list<ReferencedReelAsset> const & refs,
	int64_t duration,
	boost::filesystem::path output_dcp,
	bool ensure_subtitles,
	set<DCPTextTrack> ensure_closed_captions
	) const
{
	auto subtitle = maybe_add_text<dcp::ReelInteropTextAsset, dcp::ReelSMPTETextAsset, dcp::ReelTextAsset> (
		_subtitle_asset, dcp::TextType::OPEN_SUBTITLE, duration, reel, _reel_index, _reel_count, _content_summary, refs, film(), _period, output_dcp, _text_only
		);

	if (!subtitle && ensure_subtitles) {
		/* We had no subtitle asset, but we've been asked to make sure there is one */
		subtitle = maybe_add_text<dcp::ReelInteropTextAsset, dcp::ReelSMPTETextAsset, dcp::ReelTextAsset>(
			empty_text_asset(TextType::OPEN_SUBTITLE, optional<DCPTextTrack>(), true),
			dcp::TextType::OPEN_SUBTITLE,
			duration,
			reel,
			_reel_index,
			_reel_count,
			_content_summary,
			refs,
			film(),
			_period,
			output_dcp,
			_text_only
			);
	}

	if (subtitle) {
		/* We have a subtitle asset that we either made or are referencing */
		if (auto main_language = film()->subtitle_languages().first) {
			subtitle->set_language (*main_language);
		}
	}

	for (auto const& i: _closed_caption_assets) {
		auto a = maybe_add_text<dcp::ReelInteropTextAsset, dcp::ReelSMPTETextAsset, dcp::ReelTextAsset>(
			i.second, dcp::TextType::CLOSED_CAPTION, duration, reel, _reel_index, _reel_count, _content_summary, refs, film(), _period, output_dcp, _text_only
			);
		DCPOMATIC_ASSERT (a);
		a->set_annotation_text (i.first.name);
		if (i.first.language) {
			a->set_language (i.first.language.get());
		}

		ensure_closed_captions.erase (i.first);
	}

	/* Make empty tracks for anything we've been asked to ensure but that we haven't added */
	for (auto i: ensure_closed_captions) {
		auto a = maybe_add_text<dcp::ReelInteropTextAsset, dcp::ReelSMPTETextAsset, dcp::ReelTextAsset>(
			empty_text_asset(TextType::CLOSED_CAPTION, i, true),
			dcp::TextType::CLOSED_CAPTION,
			duration,
			reel,
			_reel_index,
			_reel_count,
			_content_summary,
			refs,
			film(),
			_period,
			output_dcp,
			_text_only
			);
		DCPOMATIC_ASSERT (a);
		a->set_annotation_text (i.name);
		if (i.language) {
			a->set_language (i.language.get());
		}
	}
}


void
ReelWriter::create_reel_markers (shared_ptr<dcp::Reel> reel) const
{
	auto markers = film()->markers();
	film()->add_ffoc_lfoc(markers);
	Film::Markers reel_markers;
	for (auto const& i: markers) {
		if (_period.contains(i.second)) {
			reel_markers[i.first] = i.second;
		}
	}

	if (!reel_markers.empty ()) {
		auto ma = make_shared<dcp::ReelMarkersAsset>(dcp::Fraction(film()->video_frame_rate(), 1), reel->duration());
		for (auto const& i: reel_markers) {
			DCPTime relative = i.second - _period.from;
			auto hmsf = relative.split (film()->video_frame_rate());
			ma->set (i.first, dcp::Time(hmsf.h, hmsf.m, hmsf.s, hmsf.f, film()->video_frame_rate()));
		}
		reel->add (ma);
	}
}


/** @param ensure_subtitles true to make sure the reel has a subtitle asset.
 *  @param ensure_closed_captions make sure the reel has these closed caption tracks.
 */
shared_ptr<dcp::Reel>
ReelWriter::create_reel (
	list<ReferencedReelAsset> const & refs,
	boost::filesystem::path output_dcp,
	bool ensure_subtitles,
	set<DCPTextTrack> ensure_closed_captions
	)
{
	LOG_GENERAL ("create_reel for %1-%2; %3 of %4", _period.from.get(), _period.to.get(), _reel_index, _reel_count);

	auto reel = make_shared<dcp::Reel>();

	/* This is a bit of a hack; in the strange `_text_only' mode we have no picture, so we don't know
	 * how long the subtitle / CCAP assets should be.  However, since we're only writing them to see
	 * how big they are, we don't care about that.
	 */
	int64_t duration = 0;
	if (!_text_only) {
		auto reel_picture_asset = create_reel_picture (reel, refs);
		duration = reel_picture_asset->actual_duration ();
		create_reel_sound (reel, refs);
		if (!film()->interop()) {
			create_reel_markers(reel);
		}
	}

	create_reel_text(reel, refs, duration, output_dcp, ensure_subtitles, ensure_closed_captions);

	if (_atmos_asset) {
		reel->add (make_shared<dcp::ReelAtmosAsset>(_atmos_asset, 0));
	}

	return reel;
}


/** @param set_progress Method to call with progress; first parameter is the number of bytes
 *  done, second parameter is the number of bytes in total.
 */
void
ReelWriter::calculate_digests(std::function<void (int64_t, int64_t)> set_progress)
try
{
	vector<shared_ptr<const dcp::Asset>> assets;

	if (_j2k_picture_asset) {
		assets.push_back(_j2k_picture_asset);
	}
	if (_mpeg2_picture_asset) {
		assets.push_back(_mpeg2_picture_asset);
	}
	if (_sound_asset) {
		assets.push_back(_sound_asset);
	}
	if (_atmos_asset) {
		assets.push_back(_atmos_asset);
	}

	int64_t total_size = 0;
	for (auto asset: assets) {
		total_size += asset->file() ? boost::filesystem::file_size(*asset->file()) : 0;
	}

	int64_t total_done = 0;
	for (auto asset: assets) {
		asset->hash([&total_done, total_size, set_progress](int64_t done, int64_t) {
			set_progress(total_done + done, total_size);
		});
		total_done += asset->file() ? boost::filesystem::file_size(*asset->file()) : 0;
	}

} catch (boost::thread_interrupted) {
	/* set_progress contains an interruption_point, so any of these methods
	 * may throw thread_interrupted, at which point we just give up.
	 */
}


Frame
ReelWriter::start () const
{
	return _period.from.frames_floor (film()->video_frame_rate());
}


void
ReelWriter::write (shared_ptr<const AudioBuffers> audio)
{
	if (!_sound_asset_writer) {
		return;
	}

	DCPOMATIC_ASSERT (audio);
	_sound_asset_writer->write(audio->data(), audio->channels(), audio->frames());
}


shared_ptr<dcp::TextAsset>
ReelWriter::empty_text_asset (TextType type, optional<DCPTextTrack> track, bool with_dummy) const
{
	shared_ptr<dcp::TextAsset> asset;
	optional<string> font;

	auto lang = film()->subtitle_languages();
	if (film()->interop()) {
		auto s = make_shared<dcp::InteropTextAsset>();
		s->set_movie_title (film()->name());
		if (type == TextType::OPEN_SUBTITLE) {
			s->set_language (lang.first ? lang.first->to_string() : "Unknown");
		} else if (track->language) {
			s->set_language (track->language->to_string());
		}
		s->set_reel_number(fmt::to_string(_reel_index + 1));
		asset = s;
	} else {
		auto s = make_shared<dcp::SMPTETextAsset>();
		s->set_content_title_text (film()->name());
		s->set_metadata (mxf_metadata());
		if (type == TextType::OPEN_SUBTITLE && lang.first) {
			s->set_language (*lang.first);
		} else if (track && track->language) {
			s->set_language (dcp::LanguageTag(track->language->to_string()));
		}
		s->set_edit_rate (dcp::Fraction (film()->video_frame_rate(), 1));
		s->set_reel_number (_reel_index + 1);
		s->set_time_code_rate (film()->video_frame_rate());
		s->set_start_time (dcp::Time ());
		if (film()->encrypted()) {
			s->set_key (film()->key());
		}
		asset = s;
	}

	if (with_dummy) {
		asset->add(
			std::make_shared<dcp::TextString>(
				font,
				false,
				false,
				false,
				dcp::Colour(),
				42,
				1.0,
				dcp::Time(0, 0, 0, 0, 24),
				dcp::Time(0, 0, 1, 0, 24),
				0.5,
				dcp::HAlign::CENTER,
				0.5,
				dcp::VAlign::CENTER,
				0,
				dcp::Direction::LTR,
				" ",
				dcp::Effect::NONE,
				dcp::Colour(),
				dcp::Time(),
				dcp::Time(),
				0,
				std::vector<dcp::Ruby>()
				)
		       );

		if (!film()->interop()) {
			/* We must have a LoadFont since we have a Text */
			font = "font";
			asset->ensure_font(*font, _default_font);
		}
	}

	return asset;
}


float
ReelWriter::convert_vertical_position(StringText const& subtitle, dcp::SubtitleStandard to) const
{
	if (dcp::uses_baseline(subtitle.valign_standard) == dcp::uses_baseline(to)) {
		/* The from and to standards use the same alignment reference */
		return subtitle.v_position();
	}

	auto const baseline_to_bottom = _font_metrics.baseline_to_bottom(subtitle);
	auto const height = _font_metrics.height(subtitle);

	float correction = 0;
	switch (subtitle.v_align()) {
	case dcp::VAlign::TOP:
		correction = height - baseline_to_bottom;
		break;
	case dcp::VAlign::CENTER:
		correction = (height / 2) - baseline_to_bottom;
		break;
	case dcp::VAlign::BOTTOM:
		correction = baseline_to_bottom;
		break;
	}

	return subtitle.v_position() + (dcp::uses_bounding_box(subtitle.valign_standard) ? correction : -correction);
}


void
ReelWriter::write(PlayerText subs, TextType type, optional<DCPTextTrack> track, DCPTimePeriod period, FontIdMap const& fonts, shared_ptr<dcpomatic::Font> chosen_interop_font)
{
	shared_ptr<dcp::TextAsset> asset;

	switch (type) {
	case TextType::OPEN_SUBTITLE:
	case TextType::OPEN_CAPTION:
		/* We could create a separate MainCaption instead of merging these, but the Deluxe guidelines
		 * suggest not to use MainCaption.
		 */
		asset = _subtitle_asset;
		break;
	case TextType::CLOSED_CAPTION:
	case TextType::CLOSED_SUBTITLE:
		/* Similarly it is advised by Deluxe not to use ClosedSubtitle */
		DCPOMATIC_ASSERT (track);
		asset = _closed_caption_assets[*track];
		break;
	default:
		DCPOMATIC_ASSERT (false);
	}

	if (!asset) {
		asset = empty_text_asset (type, track, false);
	}

	switch (type) {
	case TextType::OPEN_SUBTITLE:
	case TextType::OPEN_CAPTION:
		_subtitle_asset = asset;
		break;
	case TextType::CLOSED_CAPTION:
	case TextType::CLOSED_SUBTITLE:
		DCPOMATIC_ASSERT (track);
		_closed_caption_assets[*track] = asset;
		break;
	default:
		DCPOMATIC_ASSERT (false);
	}

	/* timecode rate for subtitles we emit; we might as well stick to ms accuracy here, I think */
	auto const tcr = 1000;

	for (auto i: subs.string) {
		i.set_in  (dcp::Time(period.from.seconds() - _period.from.seconds(), tcr));
		i.set_out (dcp::Time(period.to.seconds() - _period.from.seconds(), tcr));
		i.set_v_position(convert_vertical_position(i, film()->interop() ? dcp::SubtitleStandard::INTEROP : dcp::SubtitleStandard::SMPTE_2014));
		auto sub = make_shared<dcp::TextString>(i);
		/* i.font is a shared_ptr<Font> which uniquely identifies the font we want,
		 * though if we are Interop we can only have one font, so we'll use the chosen
		 * one instead.
		 */
		auto font = film()->interop() ? chosen_interop_font : i.font;
		/* We can get the corresponding ID from fonts */
		auto const font_id_to_use = fonts.get(font);
		/* Give this subtitle the correct font ID */
		sub->set_font(font_id_to_use);
		asset->add(sub);
		/* Make sure the asset LoadFonts the font we just asked for */
		asset->ensure_font(font_id_to_use, font->data().get_value_or(_default_font));
	}

	for (auto i: subs.bitmap) {
		asset->add (
			make_shared<dcp::TextImage>(
				image_as_png(i.image),
				dcp::Time(period.from.seconds() - _period.from.seconds(), tcr),
				dcp::Time(period.to.seconds() - _period.from.seconds(), tcr),
				i.rectangle.x, dcp::HAlign::LEFT, i.rectangle.y, dcp::VAlign::TOP, 0,
				dcp::Time(), dcp::Time()
				)
			);
	}
}


bool
ReelWriter::existing_picture_frame_ok(dcp::File& asset_file, Frame frame)
{
	LOG_GENERAL ("Checking existing picture frame %1", frame);

	/* Read the data from the info file; for 3D we just check the left
	   frames until we find a good one.
	*/
	auto const info = J2KFrameInfo(_info_file, frame, film()->three_d() ? Eyes::LEFT : Eyes::BOTH);

	bool ok = true;

	/* Read the data from the asset and hash it */
	asset_file.seek(info.offset, SEEK_SET);
	ArrayData data (info.size);
	size_t const read = asset_file.read(data.data(), 1, data.size());
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
