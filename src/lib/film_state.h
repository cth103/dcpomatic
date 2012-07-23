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

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}
#include "scaler.h"
#include "util.h"
#include "trim_action.h"

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
		: dcp_content_type (0)
		, frames_per_second (0)
		, format (0)
		, left_crop (0)
		, right_crop (0)
		, top_crop (0)
		, bottom_crop (0)
		, scaler (Scaler::from_id ("bicubic"))
		, dcp_frames (0)
		, dcp_trim_action (CUT)
		, dcp_ab (false)
		, audio_gain (0)
		, audio_delay (0)
		, still_duration (10)
		, length (0)
		, audio_channels (0)
		, audio_sample_rate (0)
		, audio_sample_format (AV_SAMPLE_FMT_NONE)
	{}

	std::string file (std::string f) const;
	std::string dir (std::string d) const;

	std::string content_path () const;
	ContentType content_type () const;
	
	bool content_is_dvd () const;

	std::string thumb_file (int) const;
	int thumb_frame (int) const;
	
	void write_metadata (std::ofstream &) const;
	void read_metadata (std::string, std::string);

	Size cropped_size (Size) const;

	/** Complete path to directory containing the film metadata;
	    must not be relative.
	*/
	std::string directory;
	/** Name for DVD-o-matic */
	std::string name;
	/** File or directory containing content; may be relative to our directory
	 *  or an absolute path.
	 */
	std::string content;
	/** The type of content that this Film represents (feature, trailer etc.) */
	DCPContentType const * dcp_content_type;
	/** Frames per second of the source */
	float frames_per_second;
	/** The format to present this Film in (flat, scope, etc.) */
	Format const * format;
	/** Number of pixels to crop from the left-hand side of the original picture */
	int left_crop;
	/** Number of pixels to crop from the right-hand side of the original picture */
	int right_crop;
	/** Number of pixels to crop from the top of the original picture */
	int top_crop;
	/** Number of pixels to crop from the bottom of the original picture */
	int bottom_crop;
	/** Video filters that should be used when generating DCPs */
	std::vector<Filter const *> filters;
	/** Scaler algorithm to use */
	Scaler const * scaler;
	/** Number of frames to put in the DCP, or 0 for all */
	int dcp_frames;

	TrimAction dcp_trim_action;
		
	/** true to create an A/B comparison DCP, where the left half of the image
	    is the video without any filters or post-processing, and the right half
	    has the specified filters and post-processing.
	*/
	bool dcp_ab;
	/** Gain to apply to audio in dB */
	float audio_gain;
	/** Delay to apply to audio (positive moves audio later) in milliseconds */
	int audio_delay;
	/** Duration to make still-sourced films (in seconds) */
	int still_duration;

	/* Data which is cached to speed things up */

	/** Vector of frame indices for each of our `thumbnails' */
	std::vector<int> thumbs;
	/** Size, in pixels, of the source (ignoring cropping) */
	Size size;
	/** Length in frames */
	int length;
	/** Number of audio channels */
	int audio_channels;
	/** Sample rate of the audio, in Hz */
	int audio_sample_rate;
	/** Format of the audio samples */
	AVSampleFormat audio_sample_format;
	/** MD5 digest of our content file */
	std::string content_digest;

private:
	std::string thumb_file_for_frame (int) const;
};

#endif
