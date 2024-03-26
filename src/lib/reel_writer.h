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


#include "atmos_metadata.h"
#include "dcp_text_track.h"
#include "dcpomatic_time.h"
#include "enum_indexed_vector.h"
#include "font_id_map.h"
#include "player_text.h"
#include "referenced_reel_asset.h"
#include "render_text.h"
#include "text_type.h"
#include "weak_film.h"
#include <dcp/atmos_asset_writer.h>
#include <dcp/file.h>
#include <dcp/j2k_picture_asset_writer.h>


class AudioBuffers;
class Film;
class InfoFileHandle;
class Job;
struct write_frame_info_test;

namespace dcp {
	class AtmosAsset;
	class MonoJ2KPictureAsset;
	class MonoJ2KPictureAssetWriter;
	class J2KPictureAsset;
	class J2KPictureAssetWriter;
	class Reel;
	class ReelAsset;
	class ReelPictureAsset;
	class SoundAsset;
	class SoundAssetWriter;
	class StereoJ2KPictureAsset;
	class StereoJ2KPictureAssetWriter;
	class SubtitleAsset;
}


class ReelWriter : public WeakConstFilm
{
public:
	ReelWriter (
		std::weak_ptr<const Film> film,
		dcpomatic::DCPTimePeriod period,
		std::shared_ptr<Job> job,
		int reel_index,
		int reel_count,
		bool text_only
		);

	void write (std::shared_ptr<const dcp::Data> encoded, Frame frame, Eyes eyes);
	void fake_write (int size);
	void repeat_write (Frame frame, Eyes eyes);
	void write (std::shared_ptr<const AudioBuffers> audio);
	void write(PlayerText text, TextType type, boost::optional<DCPTextTrack> track, dcpomatic::DCPTimePeriod period, FontIdMap const& fonts, std::shared_ptr<dcpomatic::Font> chosen_interop_font);
	void write (std::shared_ptr<const dcp::AtmosFrame> atmos, AtmosMetadata metadata);

	void finish (boost::filesystem::path output_dcp);
	std::shared_ptr<dcp::Reel> create_reel (
		std::list<ReferencedReelAsset> const & refs,
		boost::filesystem::path output_dcp,
		bool ensure_subtitles,
		std::set<DCPTextTrack> ensure_closed_captions
		);
	void calculate_digests(std::function<void (int64_t, int64_t)> set_progress);

	Frame start () const;

	dcpomatic::DCPTimePeriod period () const {
		return _period;
	}

	int first_nonexistent_frame () const {
		return _first_nonexistent_frame;
	}

private:

	friend struct ::write_frame_info_test;

	Frame check_existing_picture_asset (boost::filesystem::path asset);
	bool existing_picture_frame_ok (dcp::File& asset_file, std::shared_ptr<InfoFileHandle> info_file, Frame frame) const;
	std::shared_ptr<dcp::SubtitleAsset> empty_text_asset (TextType type, boost::optional<DCPTextTrack> track, bool with_dummy) const;

	std::shared_ptr<dcp::ReelPictureAsset> create_reel_picture (std::shared_ptr<dcp::Reel> reel, std::list<ReferencedReelAsset> const & refs) const;
	void create_reel_sound (std::shared_ptr<dcp::Reel> reel, std::list<ReferencedReelAsset> const & refs) const;
	void create_reel_text (
		std::shared_ptr<dcp::Reel> reel,
		std::list<ReferencedReelAsset> const & refs,
		int64_t duration,
		boost::filesystem::path output_dcp,
		bool ensure_subtitles,
		std::set<DCPTextTrack> ensure_closed_captions
		) const;
	void create_reel_markers (std::shared_ptr<dcp::Reel> reel) const;
	float convert_vertical_position(StringText const& subtitle, dcp::SubtitleStandard to) const;

	dcpomatic::DCPTimePeriod _period;
	/** the first picture frame index that does not already exist in our MXF */
	int _first_nonexistent_frame;
	/** the data of the last written frame, if there is one */
	EnumIndexedVector<std::shared_ptr<const dcp::Data>, Eyes> _last_written;
	/** index of this reel within the DCP (starting from 0) */
	int _reel_index;
	/** number of reels in the DCP */
	int _reel_count;
	boost::optional<std::string> _content_summary;
	std::weak_ptr<Job> _job;
	bool _text_only;

	dcp::ArrayData _default_font;

	std::shared_ptr<dcp::J2KPictureAsset> _picture_asset;
	/** picture asset writer, or 0 if we are not writing any picture because we already have one */
	std::shared_ptr<dcp::J2KPictureAssetWriter> _picture_asset_writer;
	std::shared_ptr<dcp::SoundAsset> _sound_asset;
	std::shared_ptr<dcp::SoundAssetWriter> _sound_asset_writer;
	std::shared_ptr<dcp::SubtitleAsset> _subtitle_asset;
	std::map<DCPTextTrack, std::shared_ptr<dcp::SubtitleAsset>> _closed_caption_assets;
	std::shared_ptr<dcp::AtmosAsset> _atmos_asset;
	std::shared_ptr<dcp::AtmosAssetWriter> _atmos_asset_writer;

	mutable FontMetrics _font_metrics;
};
