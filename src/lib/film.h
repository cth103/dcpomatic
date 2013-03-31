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
 *  @brief A representation of a piece of video (with sound), including naming,
 *  the source content file, and how it should be presented in a DCP.
 */

#ifndef DVDOMATIC_FILM_H
#define DVDOMATIC_FILM_H

#include <string>
#include <vector>
#include <inttypes.h>
#include <boost/thread/mutex.hpp>
#include <boost/thread.hpp>
#include <boost/signals2.hpp>
#include <boost/enable_shared_from_this.hpp>
extern "C" {
#include <libavcodec/avcodec.h>
}
#include "dcp_content_type.h"
#include "util.h"
#include "dci_metadata.h"

class Format;
class Job;
class Filter;
class Log;
class ExamineContentJob;
class AnalyseAudioJob;
class ExternalAudioStream;
class Content;
class Playlist;

/** @class Film
 *  @brief A representation of a video, maybe with sound.
 *
 *  A representation of a piece of video (maybe with sound), including naming,
 *  the source content file, and how it should be presented in a DCP.
 */
class Film : public boost::enable_shared_from_this<Film>
{
public:
	Film (std::string d, bool must_exist = true);
	Film (Film const &);
	~Film ();

	std::string info_dir () const;
	std::string j2c_path (int f, bool t) const;
	std::string info_path (int f) const;
	std::string video_mxf_dir () const;
	std::string video_mxf_filename () const;
	std::string audio_analysis_path () const;

	void examine_content (boost::shared_ptr<Content>);
	void analyse_audio ();
	void send_dcp_to_tms ();

	void make_dcp ();

	/** @return Logger.
	 *  It is safe to call this from any thread.
	 */
	boost::shared_ptr<Log> log () const {
		return _log;
	}

	int encoded_frames () const;
	
	std::string file (std::string f) const;
	std::string dir (std::string d) const;

	int target_audio_sample_rate () const;
	
	void write_metadata () const;
	void read_metadata ();

	libdcp::Size cropped_size (libdcp::Size) const;
	std::string dci_name (bool if_created_now) const;
	std::string dcp_name (bool if_created_now = false) const;

	/** @return true if our state has changed since we last saved it */
	bool dirty () const {
		return _dirty;
	}

	void set_dci_date_today ();

	bool have_dcp () const;

	boost::shared_ptr<Playlist> playlist () const;

	/** Identifiers for the parts of our state;
	    used for signalling changes.
	*/
	enum Property {
		NONE,
		NAME,
		USE_DCI_NAME,
		TRUST_CONTENT_HEADERS,
		CONTENT,
		DCP_CONTENT_TYPE,
		FORMAT,
		CROP,
		FILTERS,
		SCALER,
		TRIM_START,
		TRIM_END,
		DCP_AB,
		AUDIO_GAIN,
		AUDIO_DELAY,
		STILL_DURATION,
		SUBTITLE_STREAM,
		WITH_SUBTITLES,
		SUBTITLE_OFFSET,
		SUBTITLE_SCALE,
		COLOUR_LUT,
		J2K_BANDWIDTH,
		DCI_METADATA,
		DCP_FRAME_RATE
	};


	/* GET */

	std::string directory () const {
		boost::mutex::scoped_lock lm (_directory_mutex);
		return _directory;
	}

	std::string name () const {
		boost::mutex::scoped_lock lm (_state_mutex);
		return _name;
	}

	bool use_dci_name () const {
		boost::mutex::scoped_lock lm (_state_mutex);
		return _use_dci_name;
	}

	bool trust_content_headers () const {
		boost::mutex::scoped_lock lm (_state_mutex);
		return _trust_content_headers;
	}

	std::list<boost::shared_ptr<Content> > content () const {
		boost::mutex::scoped_lock lm (_state_mutex);
		return _content;
	}

	DCPContentType const * dcp_content_type () const {
		boost::mutex::scoped_lock lm (_state_mutex);
		return _dcp_content_type;
	}

	Format const * format () const {
		boost::mutex::scoped_lock lm (_state_mutex);
		return _format;
	}

	Crop crop () const {
		boost::mutex::scoped_lock lm (_state_mutex);
		return _crop;
	}

	std::vector<Filter const *> filters () const {
		boost::mutex::scoped_lock lm (_state_mutex);
		return _filters;
	}

	Scaler const * scaler () const {
		boost::mutex::scoped_lock lm (_state_mutex);
		return _scaler;
	}

	int trim_start () const {
		boost::mutex::scoped_lock lm (_state_mutex);
		return _trim_start;
	}

	int trim_end () const {
		boost::mutex::scoped_lock lm (_state_mutex);
		return _trim_end;
	}

	bool dcp_ab () const {
		boost::mutex::scoped_lock lm (_state_mutex);
		return _dcp_ab;
	}

	float audio_gain () const {
		boost::mutex::scoped_lock lm (_state_mutex);
		return _audio_gain;
	}

	int audio_delay () const {
		boost::mutex::scoped_lock lm (_state_mutex);
		return _audio_delay;
	}

	bool with_subtitles () const {
		boost::mutex::scoped_lock lm (_state_mutex);
		return _with_subtitles;
	}

	int subtitle_offset () const {
		boost::mutex::scoped_lock lm (_state_mutex);
		return _subtitle_offset;
	}

	float subtitle_scale () const {
		boost::mutex::scoped_lock lm (_state_mutex);
		return _subtitle_scale;
	}

	int colour_lut () const {
		boost::mutex::scoped_lock lm (_state_mutex);
		return _colour_lut;
	}

	int j2k_bandwidth () const {
		boost::mutex::scoped_lock lm (_state_mutex);
		return _j2k_bandwidth;
	}

	DCIMetadata dci_metadata () const {
		boost::mutex::scoped_lock lm (_state_mutex);
		return _dci_metadata;
	}

	int dcp_frame_rate () const {
		boost::mutex::scoped_lock lm (_state_mutex);
		return _dcp_frame_rate;
	}

	/* SET */

	void set_directory (std::string);
	void set_name (std::string);
	void set_use_dci_name (bool);
	void set_trust_content_headers (bool);
	void add_content (boost::shared_ptr<Content>);
	void set_dcp_content_type (DCPContentType const *);
	void set_format (Format const *);
	void set_crop (Crop);
	void set_left_crop (int);
	void set_right_crop (int);
	void set_top_crop (int);
	void set_bottom_crop (int);
	void set_filters (std::vector<Filter const *>);
	void set_scaler (Scaler const *);
	void set_trim_start (int);
	void set_trim_end (int);
	void set_dcp_ab (bool);
	void set_audio_gain (float);
	void set_audio_delay (int);
	void set_still_duration (int);
	void set_with_subtitles (bool);
	void set_subtitle_offset (int);
	void set_subtitle_scale (float);
	void set_colour_lut (int);
	void set_j2k_bandwidth (int);
	void set_dci_metadata (DCIMetadata);
	void set_dcp_frame_rate (int);

	/** Emitted when some property has changed */
	mutable boost::signals2::signal<void (Property)> Changed;

	boost::signals2::signal<void ()> AudioAnalysisSucceeded;

	/** Current version number of the state file */
	static int const state_version;

private:
	
	/** Log to write to */
	boost::shared_ptr<Log> _log;

	/** Any running AnalyseAudioJob, or 0 */
	boost::shared_ptr<AnalyseAudioJob> _analyse_audio_job;

	void signal_changed (Property);
	void examine_content_finished ();
	void analyse_audio_finished ();
	std::string video_state_identifier () const;

	/** Complete path to directory containing the film metadata;
	 *  must not be relative.
	 */
	std::string _directory;
	/** Mutex for _directory */
	mutable boost::mutex _directory_mutex;
	
	/** Name for DVD-o-matic */
	std::string _name;
	/** True if a auto-generated DCI-compliant name should be used for our DCP */
	bool _use_dci_name;
	typedef std::list<boost::shared_ptr<Content> > ContentList;
	ContentList _content;
	bool _trust_content_headers;
	/** The type of content that this Film represents (feature, trailer etc.) */
	DCPContentType const * _dcp_content_type;
	/** The format to present this Film in (flat, scope, etc.) */
	Format const * _format;
	/** The crop to apply to the source */
	Crop _crop;
	/** Video filters that should be used when generating DCPs */
	std::vector<Filter const *> _filters;
	/** Scaler algorithm to use */
	Scaler const * _scaler;
	/** Frames to trim off the start of the DCP */
	int _trim_start;
	/** Frames to trim off the end of the DCP */
	int _trim_end;
	/** true to create an A/B comparison DCP, where the left half of the image
	    is the video without any filters or post-processing, and the right half
	    has the specified filters and post-processing.
	*/
	bool _dcp_ab;
	/** Gain to apply to audio in dB */
	float _audio_gain;
	/** Delay to apply to audio (positive moves audio later) in milliseconds */
	int _audio_delay;
	/** True if subtitles should be shown for this film */
	bool _with_subtitles;
	/** y offset for placing subtitles, in source pixels; +ve is further down
	    the frame, -ve is further up.
	*/
	int _subtitle_offset;
	/** scale factor to apply to subtitles */
	float _subtitle_scale;
	/** index of colour LUT to use when converting RGB to XYZ.
	 *  0: sRGB
	 *  1: Rec 709
	 */
	int _colour_lut;
	/** bandwidth for J2K files in bits per second */
	int _j2k_bandwidth;

	/** DCI naming stuff */
	DCIMetadata _dci_metadata;
	/** The date that we should use in a DCI name */
	boost::gregorian::date _dci_date;
	/** Frames per second to run our DCP at */
	int _dcp_frame_rate;

	/** true if our state has changed since we last saved it */
	mutable bool _dirty;

	/** Mutex for all state except _directory */
	mutable boost::mutex _state_mutex;

	friend class paths_test;
	friend class film_metadata_test;
};

#endif
