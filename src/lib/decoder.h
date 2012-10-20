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

/** @file  src/decoder.h
 *  @brief Parent class for decoders of content.
 */

#ifndef DVDOMATIC_DECODER_H
#define DVDOMATIC_DECODER_H

#include <vector>
#include <string>
#include <stdint.h>
#include <boost/shared_ptr.hpp>
#include <sigc++/sigc++.h>
#include "util.h"
#include "stream.h"

class Job;
class FilmState;
class Options;
class Image;
class Log;
class DelayLine;
class TimedSubtitle;
class Subtitle;

/** @class Decoder.
 *  @brief Parent class for decoders of content.
 *
 *  These classes can be instructed run through their content
 *  (by calling ::go), and they emit signals when video or audio data is ready for something else
 *  to process.
 */
class Decoder
{
public:
	Decoder (boost::shared_ptr<const FilmState>, boost::shared_ptr<const Options>, Job *, Log *, bool, bool);
	virtual ~Decoder ();

	/* Methods to query our input video */

	/** @return length in video frames */
	virtual int length_in_frames () const = 0;
	/** @return video frames per second, or 0 if unknown */
	virtual float frames_per_second () const = 0;
	/** @return native size in pixels */
	virtual Size native_size () const = 0;
	/** @return number of audio channels */
	virtual int audio_channels () const = 0;
	/** @return audio sampling rate in Hz */
	virtual int audio_sample_rate () const = 0;
	/** @return format of audio samples */
	virtual AVSampleFormat audio_sample_format () const = 0;
	virtual int64_t audio_channel_layout () const = 0;
	virtual bool has_subtitles () const = 0;

	void process_begin ();
	bool pass ();
	void process_end ();
	void go ();

	/** @return the index of the last video frame to be processed */
	int last_video_frame () const {
		return _video_frame;
	}

	virtual std::vector<Stream> audio_streams () const {
		return std::vector<Stream> ();
	}
	
	virtual std::vector<Stream> subtitle_streams () const {
		return std::vector<Stream> ();
	}

	virtual void set_audio_stream (Stream s) {}
	virtual void set_subtitle_stream (Stream s) {}
	
	/** Emitted when a video frame is ready.
	 *  First parameter is the frame.
	 *  Second parameter is its index within the content.
	 *  Third parameter is either 0 or a subtitle that should be on this frame.
	 */
	sigc::signal<void, boost::shared_ptr<Image>, int, boost::shared_ptr<Subtitle> > Video;

	/** Emitted when some audio data is ready */
	sigc::signal<void, boost::shared_ptr<AudioBuffers> > Audio;
	
protected:
	/** perform a single pass at our content */
	virtual bool do_pass () = 0;
	virtual PixelFormat pixel_format () const = 0;
	virtual int time_base_numerator () const = 0;
	virtual int time_base_denominator () const = 0;
	virtual int sample_aspect_ratio_numerator () const = 0;
	virtual int sample_aspect_ratio_denominator () const = 0;
	
	void process_video (AVFrame *);
	void process_audio (uint8_t *, int);
	void process_subtitle (boost::shared_ptr<TimedSubtitle>);

	/** our FilmState */
	boost::shared_ptr<const FilmState> _fs;
	/** our options */
	boost::shared_ptr<const Options> _opt;
	/** associated Job, or 0 */
	Job* _job;
	/** log that we can write to */
	Log* _log;

	/** true to do the bare minimum of work; just run through the content.  Useful for acquiring
	 *  accurate frame counts as quickly as possible.  This generates no video or audio output.
	 */
	bool _minimal;

	/** ignore_length Ignore the content's claimed length when computing progress */
	bool _ignore_length;

private:
	void setup_video_filters ();
	void emit_audio (uint8_t* data, int size);
	int bytes_per_audio_sample () const;
	
	/** last video frame to be processed */
	int _video_frame;

	AVFilterContext* _buffer_src_context;
	AVFilterContext* _buffer_sink_context;

	bool _have_setup_video_filters;
	DelayLine* _delay_line;
	int _delay_in_bytes;

	/* Number of audio frames that we have pushed to the encoder
	   (at the DCP sample rate).
	*/
	int64_t _audio_frames_processed;

	boost::shared_ptr<TimedSubtitle> _timed_subtitle;
};

#endif
