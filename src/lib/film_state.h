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

/** @file src/film_state.h
 *  @brief The state of a Film.  This is separate from Film so that
 *  state can easily be copied and kept around for reference
 *  by long-running jobs.  This avoids the jobs getting confused
 *  by the user changing Film settings during their run.
 */

#ifndef DVDOMATIC_FILM_STATE_H
#define DVDOMATIC_FILM_STATE_H

#include <sigc++/signal.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}
#include "scaler.h"
#include "util.h"
#include "trim_action.h"
#include "stream.h"

class Format;
class DCPContentType;
class Filter;

/** @class FilmState
 *  @brief The state of a Film.
 *
 *  This is separate from Film so that state can easily be copied and
 *  kept around for reference by long-running jobs.  This avoids the
 *  jobs getting confused by the user changing Film settings during
 *  their run.
 */

class FilmState
{
public:
	FilmState ()
		: _use_dci_name (false)
		, _dcp_content_type (0)
		, _format (0)
		, _scaler (Scaler::from_id ("bicubic"))
		, _dcp_frames (0)
		, _dcp_trim_action (CUT)
		, _dcp_ab (false)
		, _audio_stream (-1)
		, _audio_gain (0)
		, _audio_delay (0)
		, _still_duration (10)
		, _subtitle_stream (-1)
		, _with_subtitles (false)
		, _subtitle_offset (0)
		, _subtitle_scale (1)
		, _length (0)
		, _audio_sample_rate (0)
		, _has_subtitles (false)
		, _frames_per_second (0)
		, _dirty (false)
	{}

	FilmState (FilmState const & o)
		: _directory         (o._directory)
		, _name              (o._name)
		, _use_dci_name      (o._use_dci_name)
		, _content           (o._content)
		, _dcp_content_type  (o._dcp_content_type)
		, _format            (o._format)
		, _crop              (o._crop)
		, _filters           (o._filters)
		, _scaler            (o._scaler)
		, _dcp_frames        (o._dcp_frames)
		, _dcp_trim_action   (o._dcp_trim_action)
		, _dcp_ab            (o._dcp_ab)
		, _audio_stream      (o._audio_stream)
		, _audio_gain        (o._audio_gain)
		, _audio_delay       (o._audio_delay)
		, _still_duration    (o._still_duration)
		, _subtitle_stream   (o._subtitle_stream)
		, _with_subtitles    (o._with_subtitles)
		, _subtitle_offset   (o._subtitle_offset)
		, _subtitle_scale    (o._subtitle_scale)
		, _audio_language    (o._audio_language)
		, _subtitle_language (o._subtitle_language)
		, _territory         (o._territory)
		, _rating            (o._rating)
		, _studio            (o._studio)
		, _facility          (o._facility)
		, _package_type      (o._package_type)
		, _thumbs            (o._thumbs)
		, _size              (o._size)
		, _length            (o._length)
		, _audio_sample_rate (o._audio_sample_rate)
		, _content_digest    (o._content_digest)
		, _has_subtitles     (o._has_subtitles)
		, _audio_streams     (o._audio_streams)
		, _subtitle_streams  (o._subtitle_streams)
		, _frames_per_second (o._frames_per_second)
		, _dirty             (o._dirty)
	{}

	virtual ~FilmState () {}

	std::string file (std::string f) const;
	std::string dir (std::string d) const;

	std::string content_path () const;
	ContentType content_type () const;
	
	bool content_is_dvd () const;

	std::string thumb_file (int) const;
	std::string thumb_base (int) const;
	int thumb_frame (int) const;

	int target_sample_rate () const;
	
	void write_metadata () const;
	void read_metadata ();

	Size cropped_size (Size) const;
	int dcp_length () const;
	std::string dci_name () const;

	std::string dcp_name () const;

	boost::shared_ptr<FilmState> state_copy () const;

	bool dirty () const {
		return _dirty;
	}

	int audio_channels () const;

	enum Property {
		NONE,
		NAME,
		USE_DCI_NAME,
		CONTENT,
		DCP_CONTENT_TYPE,
		FORMAT,
		CROP,
		FILTERS,
		SCALER,
		DCP_FRAMES,
		DCP_TRIM_ACTION,
		DCP_AB,
		AUDIO_STREAM,
		AUDIO_GAIN,
		AUDIO_DELAY,
		STILL_DURATION,
		SUBTITLE_STREAM,
		WITH_SUBTITLES,
		SUBTITLE_OFFSET,
		SUBTITLE_SCALE,
		DCI_METADATA,
		THUMBS,
		SIZE,
		LENGTH,
		AUDIO_SAMPLE_RATE,
		HAS_SUBTITLES,
		AUDIO_STREAMS,
		SUBTITLE_STREAMS,
		FRAMES_PER_SECOND,
	};


	/* GET */

	std::string directory () const {
		return _directory;
	}

	std::string name () const {
		return _name;
	}

	bool use_dci_name () const {
		return _use_dci_name;
	}

	std::string content () const {
		return _content;
	}

	DCPContentType const * dcp_content_type () const {
		return _dcp_content_type;
	}

	Format const * format () const {
		return _format;
	}

	Crop crop () const {
		return _crop;
	}

	std::vector<Filter const *> filters () const {
		return _filters;
	}

	Scaler const * scaler () const {
		return _scaler;
	}

	int dcp_frames () const {
		return _dcp_frames;
	}

	TrimAction dcp_trim_action () const {
		return _dcp_trim_action;
	}

	bool dcp_ab () const {
		return _dcp_ab;
	}

	int audio_stream_index () const {
		return _audio_stream;
	}

	AudioStream audio_stream () const {
		assert (_audio_stream < int (_audio_streams.size()));
		return _audio_streams[_audio_stream];
	}
	
	float audio_gain () const {
		return _audio_gain;
	}

	int audio_delay () const {
		return _audio_delay;
	}

	int still_duration () const {
		return _still_duration;
	}

	int subtitle_stream_index () const {
		return _subtitle_stream;
	}

	SubtitleStream subtitle_stream () const {
		assert (_subtitle_stream < int (_subtitle_streams.size()));
		return _subtitle_streams[_subtitle_stream];
	}

	bool with_subtitles () const {
		return _with_subtitles;
	}

	int subtitle_offset () const {
		return _subtitle_offset;
	}

	float subtitle_scale () const {
		return _subtitle_scale;
	}

	std::string audio_language () const {
		return _audio_language;
	}
	
	std::string subtitle_language () const {
		return _subtitle_language;
	}
	
	std::string territory () const {
		return _territory;
	}
	
	std::string rating () const {
		return _rating;
	}
	
	std::string studio () const {
		return _studio;
	}
	
	std::string facility () const {
		return _facility;
	}
	
	std::string package_type () const {
		return _package_type;
	}

	std::vector<int> thumbs () const {
		return _thumbs;
	}
	
	Size size () const {
		return _size;
	}

	int length () const {
		return _length;
	}

	int audio_sample_rate () const {
		return _audio_sample_rate;
	}
	
	std::string content_digest () const {
		return _content_digest;
	}
	
	bool has_subtitles () const {
		return _has_subtitles;
	}

	std::vector<AudioStream> audio_streams () const {
		return _audio_streams;
	}

	std::vector<SubtitleStream> subtitle_streams () const {
		return _subtitle_streams;
	}
	
	float frames_per_second () const {
		return _frames_per_second;
	}

	
	/* SET */

	void set_directory (std::string);
	void set_name (std::string);
	void set_use_dci_name (bool);
	virtual void set_content (std::string);
	void set_dcp_content_type (DCPContentType const *);
	void set_format (Format const *);
	void set_crop (Crop);
	void set_left_crop (int);
	void set_right_crop (int);
	void set_top_crop (int);
	void set_bottom_crop (int);
	void set_filters (std::vector<Filter const *>);
	void set_scaler (Scaler const *);
	void set_dcp_frames (int);
	void set_dcp_trim_action (TrimAction);
	void set_dcp_ab (bool);
	void set_audio_stream (int);
	void set_audio_gain (float);
	void set_audio_delay (int);
	void set_still_duration (int);
	void set_subtitle_stream (int);
	void set_with_subtitles (bool);
	void set_subtitle_offset (int);
	void set_subtitle_scale (float);
	void set_audio_language (std::string);
	void set_subtitle_language (std::string);
	void set_territory (std::string);
	void set_rating (std::string);
	void set_studio (std::string);
	void set_facility (std::string);
	void set_package_type (std::string);
	void set_thumbs (std::vector<int>);
	void set_size (Size);
	void set_length (int);
	void set_audio_channels (int);
	void set_audio_sample_rate (int);
	void set_content_digest (std::string);
	void set_has_subtitles (bool);
	void set_audio_streams (std::vector<AudioStream>);
	void set_subtitle_streams (std::vector<SubtitleStream>);
	void set_frames_per_second (float);

	/** Emitted when some property has changed */
	mutable sigc::signal1<void, Property> Changed;
	
private:	

	std::string thumb_file_for_frame (int) const;
	std::string thumb_base_for_frame (int) const;
	void signal_changed (Property);
	
	/** Complete path to directory containing the film metadata;
	 *  must not be relative.
	 */
	std::string _directory;
	/** Name for DVD-o-matic */
	std::string _name;
	/** True if a auto-generated DCI-compliant name should be used for our DCP */
	bool _use_dci_name;
	/** File or directory containing content; may be relative to our directory
	 *  or an absolute path.
	 */
	std::string _content;
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
	/** Number of frames to put in the DCP, or 0 for all */
	int _dcp_frames;
	/** What to do with audio when trimming DCPs */
	TrimAction _dcp_trim_action;
	/** true to create an A/B comparison DCP, where the left half of the image
	    is the video without any filters or post-processing, and the right half
	    has the specified filters and post-processing.
	*/
	bool _dcp_ab;
	/** An index into our _audio_streams vector for the stream to use for audio, or -1 if there is none */
	int _audio_stream;
	/** Gain to apply to audio in dB */
	float _audio_gain;
	/** Delay to apply to audio (positive moves audio later) in milliseconds */
	int _audio_delay;
	/** Duration to make still-sourced films (in seconds) */
	int _still_duration;
	/** An index into our _subtitle_streams vector for the stream to use for subtitles, or -1 if there is none */
	int _subtitle_stream;
	/** True if subtitles should be shown for this film */
	bool _with_subtitles;
	/** y offset for placing subtitles, in source pixels; +ve is further down
	    the frame, -ve is further up.
	*/
	int _subtitle_offset;
	/** scale factor to apply to subtitles */
	float _subtitle_scale;

	/* DCI naming stuff */
	std::string _audio_language;
	std::string _subtitle_language;
	std::string _territory;
	std::string _rating;
	std::string _studio;
	std::string _facility;
	std::string _package_type;

	/* Data which are cached to speed things up */

	/** Vector of frame indices for each of our `thumbnails' */
	std::vector<int> _thumbs;
	/** Size, in pixels, of the source (ignoring cropping) */
	Size _size;
	/** Length of the source in frames */
	int _length;
	/** Sample rate of the source audio, in Hz */
	int _audio_sample_rate;
	/** MD5 digest of our content file */
	std::string _content_digest;
	/** true if the source has subtitles */
	bool _has_subtitles;
	/** the audio streams that the source has */
	std::vector<AudioStream> _audio_streams;
	/** the subtitle streams that the source has */
	std::vector<SubtitleStream> _subtitle_streams;
	/** Frames per second of the source */
	float _frames_per_second;

	mutable bool _dirty;

	friend class paths_test;
};

#endif
