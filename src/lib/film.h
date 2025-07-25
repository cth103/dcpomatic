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


/** @file  src/film.h
 *  @brief A representation of some audio and video content, and details of
 *  how they should be presented in a DCP.
 */


#ifndef DCPOMATIC_FILM_H
#define DCPOMATIC_FILM_H


#include "change_signaller.h"
#include "dcp_text_track.h"
#include "dcpomatic_time.h"
#include "enum_indexed_vector.h"
#include "film_property.h"
#include "frame_rate_change.h"
#include "named_channel.h"
#include "ratio.h"
#include "remembered_asset.h"
#include "resolution.h"
#include "signaller.h"
#include "territory_type.h"
#include "transcode_job.h"
#include "types.h"
#include "util.h"
#include "video_encoding.h"
#include <dcp/encrypted_kdm.h>
#include <dcp/file.h>
#include <dcp/key.h>
#include <dcp/language_tag.h>
#include <dcp/rating.h>
#include <dcp/types.h>
#include <boost/filesystem.hpp>
#include <boost/signals2.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <inttypes.h>
#include <string>
#include <vector>


namespace xmlpp {
	class Document;
}

namespace dcpomatic {
	class Screen;
}

class AudioContent;
class AudioProcessor;
class Content;
class DCPContentType;
class Film;
class Job;
class Log;
class Playlist;
struct atmos_encrypted_passthrough_test;
struct isdcf_name_test;
struct isdcf_name_with_atmos;
struct isdcf_name_with_ccap;
struct isdcf_name_with_closed_subtitles;
struct ov_subs_in_vf_name;
struct recover_test_2d_encrypted;


/** @class Film
 *
 *  @brief A representation of some audio, video, subtitle and closed-caption content,
 *  and details of how they should be presented in a DCP.
 *
 *  The content of a Film is held in a Playlist (created and managed by the Film).
 */
class Film : public std::enable_shared_from_this<Film>, public Signaller
{
public:
	explicit Film(boost::optional<boost::filesystem::path> dir);
	~Film();

	Film(Film const&) = delete;
	Film& operator=(Film const&) = delete;

	boost::filesystem::path j2c_path(int, Frame, Eyes, bool) const;

	boost::filesystem::path audio_analysis_path(std::shared_ptr<const Playlist>) const;
	boost::filesystem::path subtitle_analysis_path(std::shared_ptr<const Content>) const;
	boost::filesystem::path assets_path() const;

	void send_dcp_to_tms();

	/** @return Logger.
	 *  It is safe to call this from any thread.
	 */
	std::shared_ptr<Log> log() const {
		return _log;
	}

	boost::filesystem::path file(boost::filesystem::path f) const;
	boost::filesystem::path dir(boost::filesystem::path d, bool create = true) const;

	void use_template(boost::optional<std::string> name);
	std::list<std::string> read_metadata(boost::optional<boost::filesystem::path> path = boost::optional<boost::filesystem::path>());
	void write_metadata();
	void write_metadata(boost::filesystem::path path) const;
	void write_template(boost::filesystem::path path) const;
	std::shared_ptr<xmlpp::Document> metadata(bool with_content_paths = true) const;

	void copy_from(std::shared_ptr<const Film> film);

	std::string isdcf_name(bool if_created_now) const;
	std::string dcp_name(bool if_created_now = false) const;

	/** @return true if our state has changed since we last saved it */
	bool dirty() const {
		return _dirty;
	}

	dcp::Size full_frame() const;
	dcp::Size frame_size() const;
	dcp::Size active_area() const;

	std::vector<CPLSummary> cpls() const;

	std::list<DCPTextTrack> closed_text_tracks() const;

	uint64_t required_disk_space() const;
	bool should_be_enough_disk_space(double& required, double& available) const;

	bool has_sign_language_video_channel() const;

	/* Proxies for some Playlist methods */

	ContentList content() const;
	dcpomatic::DCPTime length() const;
	int best_video_frame_rate() const;
	FrameRateChange active_frame_rate_change(dcpomatic::DCPTime) const;
	std::pair<double, double> speed_up_range(int dcp_frame_rate) const;

	dcp::DecryptedKDM make_kdm(boost::filesystem::path cpl_file, dcp::LocalTime from, dcp::LocalTime until) const;

	int state_version() const {
		return _state_version;
	}

	std::vector<NamedChannel> audio_output_channel_names() const;
	std::string audio_output_name() const;

	void repeat_content(ContentList, int);

	std::shared_ptr<const Playlist> playlist() const {
		return _playlist;
	}

	std::vector<dcpomatic::DCPTimePeriod> reels() const;
	std::list<int> mapped_audio_channels() const;

	boost::optional<dcp::LanguageTag> audio_language() const {
		return _audio_language;
	}

	/** @param burnt_in If non-null, filled with true if all open subtitles/captions are burnt in, otherwise false.
	 *  @param caption If non-null, filled with true if the first returned language is a caption rather than a subtitle.
	 *  @return Pair containing the main open subtitle/caption language, and additional languages
	 */
	std::pair<boost::optional<dcp::LanguageTag>, std::vector<dcp::LanguageTag>> open_text_languages(
		bool* burnt_in = nullptr, bool* caption = nullptr
	) const;
	/** @param caption If non-null, filled with true if the first returned language is a caption rather than a subtitle.
	 *  @return All closed subtitle/caption languages in the film.
	 */
	std::vector<dcp::LanguageTag> closed_text_languages(bool* caption = nullptr) const;

	std::string content_summary(dcpomatic::DCPTimePeriod period) const;

	bool references_dcp_video() const;
	bool references_dcp_audio() const;
	bool contains_atmos_content() const;

	bool last_written_by_earlier_than(int major, int minor, int micro) const;

	/* GET */

	boost::optional<boost::filesystem::path> directory() const {
		return _directory;
	}

	std::string name() const {
		return _name;
	}

	bool use_isdcf_name() const {
		return _use_isdcf_name;
	}

	DCPContentType const * dcp_content_type() const {
		return _dcp_content_type;
	}

	Ratio const& container() const {
		return _container;
	}

	Resolution resolution() const {
		return _resolution;
	}

	bool encrypted() const {
		return _encrypted;
	}

	dcp::Key key() const {
		return _key;
	}

	int video_bit_rate(VideoEncoding encoding) const {
		return _video_bit_rate[encoding];
	}

	/** @return The frame rate of the DCP */
	int video_frame_rate() const {
		return _video_frame_rate;
	}

	int audio_channels() const {
		return _audio_channels;
	}

	bool three_d() const {
		return _three_d;
	}

	bool sequence() const {
		return _sequence;
	}

	bool interop() const {
		return _interop;
	}

	VideoEncoding video_encoding() const {
		return _video_encoding;
	}

	bool limit_to_smpte_bv20() const {
		return _limit_to_smpte_bv20;
	}

	AudioProcessor const * audio_processor() const {
		return _audio_processor;
	}

	ReelType reel_type() const {
		return _reel_type;
	}

	int64_t reel_length() const {
		return _reel_length;
	}

	std::vector<dcpomatic::DCPTime> custom_reel_boundaries() const {
		return _custom_reel_boundaries;
	}

	std::string context_id() const {
		return _context_id;
	}

	bool reencode_j2k() const {
		return _reencode_j2k;
	}

	typedef std::map<dcp::Marker, dcpomatic::DCPTime> Markers;

	boost::optional<dcpomatic::DCPTime> marker(dcp::Marker type) const;
	Markers markers() const {
		return _markers;
	}

	std::vector<dcp::Rating> ratings() const {
		return _ratings;
	}

	std::vector<std::string> content_versions() const {
		return _content_versions;
	}

	dcp::LanguageTag name_language() const {
		return _name_language;
	}

	TerritoryType territory_type() const {
		return _territory_type;
	}

	boost::optional<dcp::LanguageTag::RegionSubtag> release_territory() const {
		return _release_territory;
	}

	boost::optional<dcp::LanguageTag> sign_language_video_language() const {
		return _sign_language_video_language;
	}

	int version_number() const {
		return _version_number;
	}

	dcp::Status status() const {
		return _status;
	}

	boost::optional<std::string> chain() const {
		return _chain;
	}

	boost::optional<std::string> distributor() const {
		return _distributor;
	}

	boost::optional<std::string> facility() const {
		return _facility;
	}

	boost::optional<std::string> studio() const {
		return _studio;
	}

	bool temp_version() const {
		return _temp_version;
	}

	bool pre_release() const {
		return _pre_release;
	}

	bool red_band() const {
		return _red_band;
	}

	bool two_d_version_of_three_d() const {
		return _two_d_version_of_three_d;
	}

	boost::optional<dcp::Luminance> luminance() const {
		return _luminance;
	}

	boost::gregorian::date isdcf_date() const {
		return _isdcf_date;
	}

	int audio_frame_rate() const {
		return _audio_frame_rate;
	}

	/* SET */

	void set_directory(boost::filesystem::path);
	void set_name(std::string);
	void set_use_isdcf_name(bool);
	void examine_and_add_content(std::vector<std::shared_ptr<Content>> const& content, bool disable_audio_analysis = false);
	void add_content(std::vector<std::shared_ptr<Content>> const& content);
	void remove_content(std::shared_ptr<Content>);
	void remove_content(ContentList);
	void move_content_earlier(std::shared_ptr<Content>);
	void move_content_later(std::shared_ptr<Content>);
	void set_dcp_content_type(DCPContentType const *);
	void set_container(Ratio c, bool user_explicit = true);
	void set_resolution(Resolution, bool user_explicit = true);
	void set_encrypted(bool);
	void set_video_bit_rate(VideoEncoding encoding, int64_t);
	void set_video_frame_rate(int rate, bool user_explicit = false);
	void set_audio_channels(int);
	void set_three_d(bool);
	void set_isdcf_date_today();
	void set_sequence(bool);
	void set_interop(bool);
	void set_video_encoding(VideoEncoding encoding);
	void set_limit_to_smpte_bv20(bool);
	void set_audio_processor(AudioProcessor const * processor);
	void set_reel_type(ReelType);
	void set_reel_length(int64_t);
	void set_custom_reel_boundaries(std::vector<dcpomatic::DCPTime> boundaries);
	void set_reencode_j2k(bool);
	void set_marker(dcp::Marker type, dcpomatic::DCPTime time);
	void unset_marker(dcp::Marker type);
	void clear_markers();
	void set_ratings(std::vector<dcp::Rating> r);
	void set_content_versions(std::vector<std::string> v);
	void set_name_language(dcp::LanguageTag lang);
	void set_territory_type(TerritoryType type);
	void set_release_territory(boost::optional<dcp::LanguageTag::RegionSubtag> region = boost::none);
	void set_sign_language_video_language(boost::optional<dcp::LanguageTag> tag);
	void set_version_number(int v);
	void set_status(dcp::Status s);
	void set_chain(boost::optional<std::string> c = boost::none);
	void set_facility(boost::optional<std::string> f = boost::none);
	void set_studio(boost::optional<std::string> s = boost::none);
	void set_temp_version(bool t);
	void set_pre_release(bool p);
	void set_red_band(bool r);
	void set_two_d_version_of_three_d(bool t);
	void set_distributor(boost::optional<std::string> d = boost::none);
	void set_luminance(boost::optional<dcp::Luminance> l = boost::none);
	void set_audio_language(boost::optional<dcp::LanguageTag> language);
	void set_audio_frame_rate(int rate);

	void add_ffoc_lfoc(Markers& markers) const;

	/** @return Reel types that are allowed given the current state of the film */
	std::vector<ReelType> possible_reel_types() const;

	void set_ui_state(std::string key, std::string value);
	boost::optional<std::string> ui_state(std::string key) const;
	void read_ui_state();

	std::vector<RememberedAsset> read_remembered_assets() const;
	void write_remembered_assets(std::vector<RememberedAsset> const& assets) const;
	std::string video_identifier() const;

	boost::filesystem::path info_file(dcpomatic::DCPTimePeriod p) const;

	/** Emitted when some property has of the Film is about to change or has changed */
	mutable boost::signals2::signal<void (ChangeType, FilmProperty)> Change;

	/** Emitted when some property of our content has changed */
	mutable boost::signals2::signal<void (ChangeType, int, bool)> ContentChange;

	/** Emitted when the film's length might have changed; this is not like a normal
	    property as its value is derived from the playlist, so it has its own signal.
	*/
	mutable boost::signals2::signal<void ()> LengthChange;

	boost::signals2::signal<void (bool)> DirtyChange;

	/** Emitted when we have something important to tell the user */
	boost::signals2::signal<void (std::string)> Message;

	/** Current version number of the state file */
	static int const current_state_version;

private:
	friend struct ::atmos_encrypted_passthrough_test;
	friend struct film_metadata_test;
	friend struct ::isdcf_name_test;
	friend struct ::isdcf_name_with_atmos;
	friend struct ::isdcf_name_with_ccap;
	friend struct ::isdcf_name_with_closed_subtitles;
	friend struct ::ov_subs_in_vf_name;
	friend struct paths_test;
	friend struct ::recover_test_2d_encrypted;
	template <class, class> friend class ChangeSignalDespatcher;

	void signal_change(ChangeType, FilmProperty);
	void signal_change(ChangeType, int);
	void playlist_change(ChangeType);
	void playlist_order_changed();
	void playlist_content_change(ChangeType type, int, bool frequent);
	void playlist_length_change();
	void maybe_add_content(std::weak_ptr<Job>, std::vector<std::weak_ptr<Content>> const& weak_content, bool disable_audio_analysis);
	void audio_analysis_finished();
	void check_settings_consistency();
	void maybe_set_container_and_resolution();
	void set_dirty(bool dirty);
	void write_ui_state() const;
	void check_reel_boundaries_for_atmos();
	std::vector<dcpomatic::DCPTimePeriod> reels_for_type(ReelType type) const;

	/** Log to write to */
	std::shared_ptr<Log> _log;
	std::shared_ptr<Playlist> _playlist;

	/** Complete path to directory containing the film metadata;
	 *  must not be relative.
	 */
	boost::optional<boost::filesystem::path> _directory;

	boost::optional<std::string> _last_written_by;

	/** Name for DCP-o-matic */
	std::string _name;
	/** True if a auto-generated ISDCF-compliant name should be used for our DCP */
	bool _use_isdcf_name;
	/** The type of content that this Film represents (feature, trailer etc.) */
	DCPContentType const * _dcp_content_type;
	/** The container to put this Film in (flat, scope, etc.) */
	Ratio _container;
	/** DCP resolution (2K or 4K) */
	Resolution _resolution;
	bool _encrypted;
	dcp::Key _key;
	/** context ID used when encrypting picture assets; we keep it so that we can
	 *  re-start picture MXF encodes.
	 */
	std::string _context_id;
	/** bit rate for encoding video using in bits per second */
	EnumIndexedVector<int64_t, VideoEncoding> _video_bit_rate;
	/** Frames per second to run our DCP at */
	int _video_frame_rate;
	/** The date that we should use in a ISDCF name */
	boost::gregorian::date _isdcf_date;
	/** Number of audio channels requested for the DCP */
	int _audio_channels;
	/** If true, the DCP will be written in 3D mode; otherwise in 2D.
	    This will be regardless of what content is on the playlist.
	*/
	bool _three_d;
	bool _sequence;
	bool _interop;
	VideoEncoding _video_encoding;
	bool _limit_to_smpte_bv20;
	AudioProcessor const * _audio_processor;
	ReelType _reel_type;
	/** Desired reel length in bytes, if _reel_type == BY_LENGTH */
	int64_t _reel_length;
	/** Reel boundaries (excluding those at the start and end, sorted in ascending order) if _reel_type == CUSTOM */
	std::vector<dcpomatic::DCPTime> _custom_reel_boundaries;
	bool _reencode_j2k;
	/** true if the user has ever explicitly set the video frame rate of this film */
	bool _user_explicit_video_frame_rate;
	bool _user_explicit_container;
	bool _user_explicit_resolution;
	std::map<dcp::Marker, dcpomatic::DCPTime> _markers;
	std::vector<dcp::Rating> _ratings;
	std::vector<std::string> _content_versions;
	dcp::LanguageTag _name_language;
	TerritoryType _territory_type = TerritoryType::SPECIFIC;
	boost::optional<dcp::LanguageTag::RegionSubtag> _release_territory;
	boost::optional<dcp::LanguageTag> _sign_language_video_language;
	int _version_number;
	dcp::Status _status;
	boost::optional<std::string> _chain;
	boost::optional<std::string> _distributor;
	boost::optional<std::string> _facility;
	boost::optional<std::string> _studio;
	bool _temp_version = false;
	bool _pre_release = false;
	bool _red_band = false;
	bool _two_d_version_of_three_d = false;
	boost::optional<dcp::Luminance> _luminance;
	boost::optional<dcp::LanguageTag> _audio_language;
	int _audio_frame_rate = 48000;

	int _state_version;

	/** true if our state has changed since we last saved it */
	mutable bool _dirty;
	/** film being used as a template, or 0 */
	std::shared_ptr<Film> _template_film;

	std::map<std::string, std::string> _ui_state;

	boost::signals2::scoped_connection _playlist_change_connection;
	boost::signals2::scoped_connection _playlist_order_changed_connection;
	boost::signals2::scoped_connection _playlist_content_change_connection;
	boost::signals2::scoped_connection _playlist_length_change_connection;
	std::list<boost::signals2::connection> _job_connections;
	std::list<boost::signals2::connection> _audio_analysis_connections;

};


typedef ChangeSignaller<Film, FilmProperty> FilmChangeSignaller;


#endif
