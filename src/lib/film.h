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
#include "frame_rate_change.h"
#include "isdcf_metadata.h"
#include "signaller.h"
#include "types.h"
#include "util.h"
#include <dcp/encrypted_kdm.h>
#include <dcp/key.h>
#include <dcp/language_tag.h>
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

class DCPContentType;
class Log;
class Content;
class Playlist;
class AudioContent;
class AudioProcessor;
class Ratio;
class Job;
class Film;
struct isdcf_name_test;
struct recover_test_2d_encrypted;
struct atmos_encrypted_passthrough_test;

class InfoFileHandle
{
public:
	InfoFileHandle (boost::mutex& mutex, boost::filesystem::path file, bool read);
	~InfoFileHandle ();

	FILE* get () const {
		return _handle;
	}

	boost::filesystem::path file () const {
		return _file;
	}

private:
	friend class Film;

	boost::mutex::scoped_lock _lock;
	FILE* _handle;
	boost::filesystem::path _file;
};

/** @class Film
 *
 *  @brief A representation of some audio, video, subtitle and closed-caption content,
 *  and details of how they should be presented in a DCP.
 *
 *  The content of a Film is held in a Playlist (created and managed by the Film).
 */
class Film : public std::enable_shared_from_this<Film>, public Signaller, public boost::noncopyable
{
public:
	explicit Film (boost::optional<boost::filesystem::path> dir);
	~Film ();

	std::shared_ptr<InfoFileHandle> info_file_handle (dcpomatic::DCPTimePeriod period, bool read) const;
	boost::filesystem::path j2c_path (int, Frame, Eyes, bool) const;
	boost::filesystem::path internal_video_asset_dir () const;
	boost::filesystem::path internal_video_asset_filename (dcpomatic::DCPTimePeriod p) const;

	boost::filesystem::path audio_analysis_path (std::shared_ptr<const Playlist>) const;
	boost::filesystem::path subtitle_analysis_path (std::shared_ptr<const Content>) const;

	void send_dcp_to_tms ();
	void make_dcp (bool gui = false, bool check = true);

	/** @return Logger.
	 *  It is safe to call this from any thread.
	 */
	std::shared_ptr<Log> log () const {
		return _log;
	}

	boost::filesystem::path file (boost::filesystem::path f) const;
	boost::filesystem::path dir (boost::filesystem::path d, bool create = true) const;

	void use_template (std::string name);
	std::list<std::string> read_metadata (boost::optional<boost::filesystem::path> path = boost::optional<boost::filesystem::path> ());
	void write_metadata () const;
	void write_metadata (boost::filesystem::path path) const;
	void write_template (boost::filesystem::path path) const;
	std::shared_ptr<xmlpp::Document> metadata (bool with_content_paths = true) const;

	void copy_from (std::shared_ptr<const Film> film);

	std::string isdcf_name (bool if_created_now) const;
	std::string dcp_name (bool if_created_now = false) const;

	/** @return true if our state has changed since we last saved it */
	bool dirty () const {
		return _dirty;
	}

	dcp::Size full_frame () const;
	dcp::Size frame_size () const;
	dcp::Size active_area () const;

	std::vector<CPLSummary> cpls () const;

	int audio_frame_rate () const;

	std::list<DCPTextTrack> closed_caption_tracks () const;

	uint64_t required_disk_space () const;
	bool should_be_enough_disk_space (double& required, double& available, bool& can_hard_link) const;

	/* Proxies for some Playlist methods */

	ContentList content () const;
	dcpomatic::DCPTime length () const;
	int best_video_frame_rate () const;
	FrameRateChange active_frame_rate_change (dcpomatic::DCPTime) const;
	std::pair<double, double> speed_up_range (int dcp_frame_rate) const;

	dcp::EncryptedKDM make_kdm (
		dcp::Certificate recipient,
		std::vector<std::string> trusted_devices,
		boost::filesystem::path cpl_file,
		dcp::LocalTime from,
		dcp::LocalTime until,
		dcp::Formulation formulation,
		bool disable_forensic_marking_picture,
		boost::optional<int> disable_forensic_marking_audio
		) const;

	int state_version () const {
		return _state_version;
	}

	std::vector<NamedChannel> audio_output_names () const;

	void repeat_content (ContentList, int);

	std::shared_ptr<const Playlist> playlist () const {
		return _playlist;
	}

	std::list<dcpomatic::DCPTimePeriod> reels () const;
	std::list<int> mapped_audio_channels () const;

	std::string content_summary (dcpomatic::DCPTimePeriod period) const;

	bool references_dcp_video () const;
	bool references_dcp_audio () const;
	bool contains_atmos_content () const;

	void set_tolerant (bool t) {
		_tolerant = t;
	}

	bool tolerant () const {
		return _tolerant;
	}

	/** Identifiers for the parts of our state;
	    used for signalling changes.
	*/
	enum class Property {
		NONE,
		NAME,
		USE_ISDCF_NAME,
		/** The playlist's content list has changed (i.e. content has been added or removed) */
		CONTENT,
		/** The order of content in the playlist has changed */
		CONTENT_ORDER,
		DCP_CONTENT_TYPE,
		CONTAINER,
		RESOLUTION,
		ENCRYPTED,
		J2K_BANDWIDTH,
		ISDCF_METADATA,
		VIDEO_FRAME_RATE,
		AUDIO_CHANNELS,
		/** The setting of _three_d has changed */
		THREE_D,
		SEQUENCE,
		INTEROP,
		AUDIO_PROCESSOR,
		REEL_TYPE,
		REEL_LENGTH,
		REENCODE_J2K,
		MARKERS,
		RATINGS,
		CONTENT_VERSIONS,
		NAME_LANGUAGE,
		AUDIO_LANGUAGE,
		RELEASE_TERRITORY,
		VERSION_NUMBER,
		STATUS,
		CHAIN,
		DISTRIBUTOR,
		FACILITY,
		LUMINANCE,
		SUBTITLE_LANGUAGES
	};


	/* GET */

	boost::optional<boost::filesystem::path> directory () const {
		return _directory;
	}

	std::string name () const {
		return _name;
	}

	bool use_isdcf_name () const {
		return _use_isdcf_name;
	}

	DCPContentType const * dcp_content_type () const {
		return _dcp_content_type;
	}

	Ratio const * container () const {
		return _container;
	}

	Resolution resolution () const {
		return _resolution;
	}

	bool encrypted () const {
		return _encrypted;
	}

	dcp::Key key () const {
		return _key;
	}

	int j2k_bandwidth () const {
		return _j2k_bandwidth;
	}

	ISDCFMetadata isdcf_metadata () const {
		return _isdcf_metadata;
	}

	/** @return The frame rate of the DCP */
	int video_frame_rate () const {
		return _video_frame_rate;
	}

	int audio_channels () const {
		return _audio_channels;
	}

	bool three_d () const {
		return _three_d;
	}

	bool sequence () const {
		return _sequence;
	}

	bool interop () const {
		return _interop;
	}

	AudioProcessor const * audio_processor () const {
		return _audio_processor;
	}

	ReelType reel_type () const {
		return _reel_type;
	}

	int64_t reel_length () const {
		return _reel_length;
	}

	std::string context_id () const {
		return _context_id;
	}

	bool reencode_j2k () const {
		return _reencode_j2k;
	}

	typedef std::map<dcp::Marker, dcpomatic::DCPTime> Markers;

	boost::optional<dcpomatic::DCPTime> marker (dcp::Marker type) const;
	Markers markers () const {
		return _markers;
	}

	std::vector<dcp::Rating> ratings () const {
		return _ratings;
	}

	std::vector<std::string> content_versions () const {
		return _content_versions;
	}

	dcp::LanguageTag name_language () const {
		return _name_language;
	}

	dcp::LanguageTag audio_language () const {
		return _audio_language;
	}

	dcp::LanguageTag::RegionSubtag release_territory () const {
		return _release_territory;
	}

	int version_number () const {
		return _version_number;
	}

	dcp::Status status () const {
		return _status;
	}

	std::string chain () const {
		return _chain;
	}

	std::string distributor () const {
		return _distributor;
	}

	std::string facility () const {
		return _facility;
	}

	dcp::Luminance luminance () const {
		return _luminance;
	}

	std::vector<dcp::LanguageTag> subtitle_languages () const {
		return _subtitle_languages;
	}

	/* SET */

	void set_directory (boost::filesystem::path);
	void set_name (std::string);
	void set_use_isdcf_name (bool);
	void examine_and_add_content (std::shared_ptr<Content> content, bool disable_audio_analysis = false);
	void add_content (std::shared_ptr<Content>);
	void remove_content (std::shared_ptr<Content>);
	void remove_content (ContentList);
	void move_content_earlier (std::shared_ptr<Content>);
	void move_content_later (std::shared_ptr<Content>);
	void set_dcp_content_type (DCPContentType const *);
	void set_container (Ratio const *, bool user_explicit = true);
	void set_resolution (Resolution, bool user_explicit = true);
	void set_encrypted (bool);
	void set_j2k_bandwidth (int);
	void set_isdcf_metadata (ISDCFMetadata);
	void set_video_frame_rate (int rate, bool user_explicit = false);
	void set_audio_channels (int);
	void set_three_d (bool);
	void set_isdcf_date_today ();
	void set_sequence (bool);
	void set_interop (bool);
	void set_audio_processor (AudioProcessor const * processor);
	void set_reel_type (ReelType);
	void set_reel_length (int64_t);
	void set_reencode_j2k (bool);
	void set_marker (dcp::Marker type, dcpomatic::DCPTime time);
	void unset_marker (dcp::Marker type);
	void clear_markers ();
	void set_ratings (std::vector<dcp::Rating> r);
	void set_content_versions (std::vector<std::string> v);
	void set_name_language (dcp::LanguageTag lang);
	void set_audio_language (dcp::LanguageTag lang);
	void set_release_territory (dcp::LanguageTag::RegionSubtag region);
	void set_version_number (int v);
	void set_status (dcp::Status s);
	void set_chain (std::string c);
	void set_facility (std::string f);
	void set_distributor (std::string d);
	void set_luminance (dcp::Luminance l);
	void set_subtitle_language (dcp::LanguageTag language);
	void unset_subtitle_language ();
	void set_subtitle_languages (std::vector<dcp::LanguageTag> languages);

	void add_ffoc_lfoc (Markers& markers) const;

	/** Emitted when some property has of the Film is about to change or has changed */
	mutable boost::signals2::signal<void (ChangeType, Property)> Change;

	/** Emitted when some property of our content has changed */
	mutable boost::signals2::signal<void (ChangeType, std::weak_ptr<Content>, int, bool)> ContentChange;

	/** Emitted when the film's length might have changed; this is not like a normal
	    property as its value is derived from the playlist, so it has its own signal.
	*/
	mutable boost::signals2::signal<void ()> LengthChange;

	/** Emitted when we have something important to tell the user */
	boost::signals2::signal<void (std::string)> Message;

	/** Current version number of the state file */
	static int const current_state_version;

private:

	friend struct ::isdcf_name_test;
	friend struct ::recover_test_2d_encrypted;
	friend struct ::atmos_encrypted_passthrough_test;
	template <class, class> friend class ChangeSignaller;

	boost::filesystem::path info_file (dcpomatic::DCPTimePeriod p) const;

	void signal_change (ChangeType, Property);
	void signal_change (ChangeType, int);
	std::string video_identifier () const;
	void playlist_change (ChangeType);
	void playlist_order_changed ();
	void playlist_content_change (ChangeType type, std::weak_ptr<Content>, int, bool frequent);
	void playlist_length_change ();
	void maybe_add_content (std::weak_ptr<Job>, std::weak_ptr<Content>, bool disable_audio_analysis);
	void audio_analysis_finished ();
	void check_settings_consistency ();
	void maybe_set_container_and_resolution ();

	static std::string const metadata_file;

	/** Log to write to */
	std::shared_ptr<Log> _log;
	std::shared_ptr<Playlist> _playlist;

	/** Complete path to directory containing the film metadata;
	 *  must not be relative.
	 */
	boost::optional<boost::filesystem::path> _directory;

	/** Name for DCP-o-matic */
	std::string _name;
	/** True if a auto-generated ISDCF-compliant name should be used for our DCP */
	bool _use_isdcf_name;
	/** The type of content that this Film represents (feature, trailer etc.) */
	DCPContentType const * _dcp_content_type;
	/** The container to put this Film in (flat, scope, etc.) */
	Ratio const * _container;
	/** DCP resolution (2K or 4K) */
	Resolution _resolution;
	bool _encrypted;
	dcp::Key _key;
	/** context ID used when encrypting picture assets; we keep it so that we can
	 *  re-start picture MXF encodes.
	 */
	std::string _context_id;
	/** bandwidth for J2K files in bits per second */
	int _j2k_bandwidth;
	/** ISDCF naming stuff */
	ISDCFMetadata _isdcf_metadata;
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
	AudioProcessor const * _audio_processor;
	ReelType _reel_type;
	/** Desired reel length in bytes, if _reel_type == REELTYPE_BY_LENGTH */
	int64_t _reel_length;
	bool _reencode_j2k;
	/** true if the user has ever explicitly set the video frame rate of this film */
	bool _user_explicit_video_frame_rate;
	bool _user_explicit_container;
	bool _user_explicit_resolution;
	std::map<dcp::Marker, dcpomatic::DCPTime> _markers;
	std::vector<dcp::Rating> _ratings;
	std::vector<std::string> _content_versions;
	dcp::LanguageTag _name_language;
	dcp::LanguageTag _audio_language;
	dcp::LanguageTag::RegionSubtag _release_territory;
	int _version_number;
	dcp::Status _status;
	std::string _chain;
	std::string _distributor;
	std::string _facility;
	dcp::Luminance _luminance;
	std::vector<dcp::LanguageTag> _subtitle_languages;

	int _state_version;

	/** true if our state has changed since we last saved it */
	mutable bool _dirty;
	/** film being used as a template, or 0 */
	std::shared_ptr<Film> _template_film;

	/** Be tolerant of errors in content (currently applies to DCP only).
	    Not saved as state.
	*/
	bool _tolerant;

	mutable boost::mutex _info_file_mutex;

	boost::signals2::scoped_connection _playlist_change_connection;
	boost::signals2::scoped_connection _playlist_order_changed_connection;
	boost::signals2::scoped_connection _playlist_content_change_connection;
	boost::signals2::scoped_connection _playlist_length_change_connection;
	std::list<boost::signals2::connection> _job_connections;
	std::list<boost::signals2::connection> _audio_analysis_connections;

	friend struct paths_test;
	friend struct film_metadata_test;
};


typedef ChangeSignaller<Film, Film::Property> FilmChangeSignaller;


#endif
