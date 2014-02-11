/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

/** @file  src/film.h
 *  @brief A representation of some audio and video content, and details of
 *  how they should be presented in a DCP.
 */

#ifndef DCPOMATIC_FILM_H
#define DCPOMATIC_FILM_H

#include <string>
#include <vector>
#include <inttypes.h>
#include <boost/signals2.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/filesystem.hpp>
#include <libdcp/key.h>
#include <libdcp/kdm.h>
#include "util.h"
#include "types.h"
#include "dci_metadata.h"

class DCPContentType;
class Log;
class Content;
class Player;
class Playlist;
class AudioContent;
class Scaler;
class Screen;

/** @class Film
 *
 *  @brief A representation of some audio and video content, and details of
 *  how they should be presented in a DCP.
 *
 *  The content of a Film is held in a Playlist (created and managed by the Film).
 */
class Film : public boost::enable_shared_from_this<Film>, public boost::noncopyable
{
public:
	Film (boost::filesystem::path);

	boost::filesystem::path info_dir () const;
	boost::filesystem::path j2c_path (int, Eyes, bool) const;
	boost::filesystem::path info_path (int, Eyes) const;
	boost::filesystem::path internal_video_mxf_dir () const;
	boost::filesystem::path internal_video_mxf_filename () const;
	boost::filesystem::path audio_analysis_path (boost::shared_ptr<const AudioContent>) const;

	boost::filesystem::path video_mxf_filename () const;
	boost::filesystem::path audio_mxf_filename () const;

	void send_dcp_to_tms ();
	void make_dcp ();

	/** @return Logger.
	 *  It is safe to call this from any thread.
	 */
	boost::shared_ptr<Log> log () const {
		return _log;
	}

	int encoded_frames () const;
	
	boost::filesystem::path file (boost::filesystem::path f) const;
	boost::filesystem::path dir (boost::filesystem::path d) const;

	void read_metadata ();
	void write_metadata () const;

	std::string dci_name (bool if_created_now) const;
	std::string dcp_name (bool if_created_now = false) const;

	/** @return true if our state has changed since we last saved it */
	bool dirty () const {
		return _dirty;
	}

	dcp::Size full_frame () const;

	std::list<boost::filesystem::path> dcps () const;

	boost::shared_ptr<Player> make_player () const;
	boost::shared_ptr<Playlist> playlist () const;

	AudioFrame audio_frame_rate () const;

	AudioFrame time_to_audio_frames (DCPTime) const;
	VideoFrame time_to_video_frames (DCPTime) const;
	DCPTime video_frames_to_time (VideoFrame) const;
	DCPTime audio_frames_to_time (AudioFrame) const;

	uint64_t required_disk_space () const;
	bool should_be_enough_disk_space (double &, double &) const;
	
	/* Proxies for some Playlist methods */

	ContentList content () const;
	DCPTime length () const;
	bool has_subtitles () const;
	VideoFrame best_video_frame_rate () const;
	FrameRateChange active_frame_rate_change (DCPTime) const;

	dcp::KDM
	make_kdm (
		boost::shared_ptr<dcp::Certificate> target,
		boost::filesystem::path dcp,
		boost::posix_time::ptime from,
		boost::posix_time::ptime until
		) const;
	
	std::list<dcp::KDM> make_kdms (
		std::list<boost::shared_ptr<Screen> >,
		boost::filesystem::path dcp,
		boost::posix_time::ptime from,
		boost::posix_time::ptime until
		) const;

	dcp::Key key () const {
		return _key;
	}

	/** Identifiers for the parts of our state;
	    used for signalling changes.
	*/
	enum Property {
		NONE,
		NAME,
		USE_DCI_NAME,
		/** The playlist's content list has changed (i.e. content has been added, moved around or removed) */
		CONTENT,
		DCP_CONTENT_TYPE,
		CONTAINER,
		RESOLUTION,
		SCALER,
		WITH_SUBTITLES,
		SIGNED,
		ENCRYPTED,
		J2K_BANDWIDTH,
		DCI_METADATA,
		VIDEO_FRAME_RATE,
		AUDIO_CHANNELS,
		/** The setting of _three_d has been changed */
		THREE_D,
		SEQUENCE_VIDEO,
		INTEROP,
	};


	/* GET */

	boost::filesystem::path directory () const {
		return _directory;
	}

	std::string name () const {
		return _name;
	}

	bool use_dci_name () const {
		return _use_dci_name;
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

	Scaler const * scaler () const {
		return _scaler;
	}

	bool with_subtitles () const {
		return _with_subtitles;
	}

	/* signed is a reserved word */
	bool is_signed () const {
		return _signed;
	}
	
	bool encrypted () const {
		return _encrypted;
	}

	int j2k_bandwidth () const {
		return _j2k_bandwidth;
	}

	DCIMetadata dci_metadata () const {
		return _dci_metadata;
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

	bool sequence_video () const {
		return _sequence_video;
	}

	bool interop () const {
		return _interop;
	}
	

	/* SET */

	void set_directory (boost::filesystem::path);
	void set_name (std::string);
	void set_use_dci_name (bool);
	void examine_and_add_content (boost::shared_ptr<Content>);
	void add_content (boost::shared_ptr<Content>);
	void remove_content (boost::shared_ptr<Content>);
	void move_content_earlier (boost::shared_ptr<Content>);
	void move_content_later (boost::shared_ptr<Content>);
	void set_dcp_content_type (DCPContentType const *);
	void set_container (Ratio const *);
	void set_resolution (Resolution);
	void set_scaler (Scaler const *);
	void set_with_subtitles (bool);
	void set_signed (bool);
	void set_encrypted (bool);
	void set_j2k_bandwidth (int);
	void set_dci_metadata (DCIMetadata);
	void set_video_frame_rate (int);
	void set_audio_channels (int);
	void set_three_d (bool);
	void set_dci_date_today ();
	void set_sequence_video (bool);
	void set_interop (bool);

	/** Emitted when some property has of the Film has changed */
	mutable boost::signals2::signal<void (Property)> Changed;

	/** Emitted when some property of our content has changed */
	mutable boost::signals2::signal<void (boost::weak_ptr<Content>, int)> ContentChanged;

	/** Current version number of the state file */
	static int const state_version;

private:

	void signal_changed (Property);
	std::string video_identifier () const;
	void playlist_changed ();
	void playlist_content_changed (boost::weak_ptr<Content>, int);
	std::string filename_safe_name () const;
	void maybe_add_content (boost::weak_ptr<Job>, boost::weak_ptr<Content>);

	/** Log to write to */
	boost::shared_ptr<Log> _log;
	boost::shared_ptr<Playlist> _playlist;

	/** Complete path to directory containing the film metadata;
	 *  must not be relative.
	 */
	boost::filesystem::path _directory;
	
	/** Name for DCP-o-matic */
	std::string _name;
	/** True if a auto-generated DCI-compliant name should be used for our DCP */
	bool _use_dci_name;
	/** The type of content that this Film represents (feature, trailer etc.) */
	DCPContentType const * _dcp_content_type;
	/** The container to put this Film in (flat, scope, etc.) */
	Ratio const * _container;
	/** DCP resolution (2K or 4K) */
	Resolution _resolution;
	/** Scaler algorithm to use */
	Scaler const * _scaler;
	/** True if subtitles should be shown for this film */
	bool _with_subtitles;
	bool _signed;
	bool _encrypted;
	/** bandwidth for J2K files in bits per second */
	int _j2k_bandwidth;
	/** DCI naming stuff */
	DCIMetadata _dci_metadata;
	/** Frames per second to run our DCP at */
	int _video_frame_rate;
	/** The date that we should use in a DCI name */
	boost::gregorian::date _dci_date;
	/** Number of audio channels to put in the DCP */
	int _audio_channels;
	/** If true, the DCP will be written in 3D mode; otherwise in 2D.
	    This will be regardless of what content is on the playlist.
	*/
	bool _three_d;
	bool _sequence_video;
	bool _interop;
	dcp::Key _key;

	/** true if our state has changed since we last saved it */
	mutable bool _dirty;

	friend class paths_test;
	friend class film_metadata_test;
};

#endif
