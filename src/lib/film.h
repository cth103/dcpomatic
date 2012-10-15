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
#include <sigc++/signal.h>
extern "C" {
#include <libavcodec/avcodec.h>
}
#include "dcp_content_type.h"
#include "film_state.h"

class Format;
class Job;
class Filter;
class Log;
class ExamineContentJob;

/** @class Film
 *  @brief A representation of a video with sound.
 *
 *  A representation of a piece of video (with sound), including naming,
 *  the source content file, and how it should be presented in a DCP.
 */
class Film
{
public:
	Film (std::string d, bool must_exist = true);
	Film (Film const &);
	~Film ();

	void write_metadata () const;

	/** @return complete path to directory containing the film metadata */
	std::string directory () const {
		return _state.directory;
	}

	std::string content () const;
	ContentType content_type () const;

	/** @return name for DVD-o-matic */
	std::string name () const {
		return _state.name;
	}

	/** @return number of pixels to crop from the sides of the original picture */
	Crop crop () const {
		return _state.crop;
	}

	/** @return the format to present this film in (flat, scope, etc.) */
	Format const * format () const {
		return _state.format;
	}

	/** @return video filters that should be used when generating DCPs */
	std::vector<Filter const *> filters () const {
		return _state.filters;
	}

	/** @return scaler algorithm to use */
	Scaler const * scaler () const {
		return _state.scaler;
	}

	/** @return number of frames to put in the DCP, or 0 for all */
	int dcp_frames () const {
		return _state.dcp_frames;
	}

	TrimAction dcp_trim_action () const {
		return _state.dcp_trim_action;
	}

	/** @return true to create an A/B comparison DCP, where the left half of the image
	 *  is the video without any filters or post-processing, and the right half
	 *  has the specified filters and post-processing.
	 */
	bool dcp_ab () const {
		return _state.dcp_ab;
	}

	float audio_gain () const {
		return _state.audio_gain;
	}

	int audio_delay () const {
		return _state.audio_delay;
	}

	int still_duration () const {
		return _state.still_duration;
	}

	bool with_subtitles () const {
		return _state.with_subtitles;
	}

	int subtitle_offset () const {
		return _state.subtitle_offset;
	}

	float subtitle_scale () const {
		return _state.subtitle_scale;
	}
	
	void set_filters (std::vector<Filter const *> const &);

	void set_scaler (Scaler const *);

	/** @return the type of content that this Film represents (feature, trailer etc.) */
	DCPContentType const * dcp_content_type () {
		return _state.dcp_content_type;
	}

	void set_dcp_frames (int);
	void set_dcp_trim_action (TrimAction);
	void set_dcp_ab (bool);
	
	void set_name (std::string);
	void set_content (std::string);
	void set_top_crop (int);
	void set_bottom_crop (int);
	void set_left_crop (int);
	void set_right_crop (int);
	void set_format (Format const *);
	void set_dcp_content_type (DCPContentType const *);
	void set_audio_gain (float);
	void set_audio_delay (int);
	void set_still_duration (int);
	void set_with_subtitles (bool);
	void set_subtitle_offset (int);
	void set_subtitle_scale (float);

	/** @return size, in pixels, of the source (ignoring cropping) */
	Size size () const {
		return _state.size;
	}

	/** @return length, in video frames */
	int length () const {
		return _state.length;
	}

	/** @return nnumber of video frames per second */
	float frames_per_second () const {
		return _state.frames_per_second;
	}

	/** @return number of audio channels */
	int audio_channels () const {
		return _state.audio_channels;
	}

	/** @return audio sample rate, in Hz */
	int audio_sample_rate () const {
		return _state.audio_sample_rate;
	}

	/** @return format of the audio samples */
	AVSampleFormat audio_sample_format () const {
		return _state.audio_sample_format;
	}

	bool has_subtitles () const {
		return _state.has_subtitles;
	}
	
	std::string j2k_dir () const;

	std::vector<std::string> audio_files () const;

	void update_thumbs_pre_gui ();
	void update_thumbs_post_gui ();
	int num_thumbs () const;
	int thumb_frame (int) const;
	std::string thumb_file (int) const;
	std::pair<Position, std::string> thumb_subtitle (int) const;

	void copy_from_dvd_post_gui ();
	void examine_content ();
	void examine_content_post_gui ();
	void send_dcp_to_tms ();
	void copy_from_dvd ();

	/** @return true if our metadata has been modified since it was last saved */
	bool dirty () const {
		return _dirty;
	}

	void make_dcp (bool, int freq = 0);

	enum Property {
		NONE,
		NAME,
		CONTENT,
		DCP_CONTENT_TYPE,
		FORMAT,
		CROP,
		FILTERS,
		SCALER,
		DCP_FRAMES,
		DCP_TRIM_ACTION,
		DCP_AB,
		AUDIO_GAIN,
		AUDIO_DELAY,
		THUMBS,
		SIZE,
		LENGTH,
		FRAMES_PER_SECOND,
		AUDIO_CHANNELS,
		AUDIO_SAMPLE_RATE,
		STILL_DURATION,
		WITH_SUBTITLES,
		SUBTITLE_OFFSET,
		SUBTITLE_SCALE
	};

	boost::shared_ptr<FilmState> state_copy () const;

	/** @return Logger.
	 *  It is safe to call this from any thread.
	 */
	Log* log () const {
		return _log;
	}

	int encoded_frames () const;

	/** Emitted when some metadata property has changed */
	mutable sigc::signal1<void, Property> Changed;
	
private:
	void read_metadata ();
	std::string metadata_file () const;
	void update_dimensions ();
	void signal_changed (Property);

	/** The majority of our state.  Kept in a separate object
	 *  so that it can easily be copied for passing onto long-running
	 *  jobs (which then have an unchanging set of parameters).
	 */
	FilmState _state;

	/** true if our metadata has changed since it was last written to disk */
	mutable bool _dirty;

	/** Log to write to */
	Log* _log;

	/** Any running ExamineContentJob, or 0 */
	boost::shared_ptr<ExamineContentJob> _examine_content_job;
};

#endif
