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

#ifndef DVDOMATIC_VIDEO_DECODER_H
#define DVDOMATIC_VIDEO_DECODER_H

#include "video_source.h"
#include "stream.h"
#include "decoder.h"

class VideoDecoder : public VideoSource, public virtual Decoder
{
public:
	VideoDecoder (boost::shared_ptr<Film>, boost::shared_ptr<const DecodeOptions>, Job *);

	/** @return video frames per second, or 0 if unknown */
	virtual float frames_per_second () const = 0;
	/** @return native size in pixels */
	virtual Size native_size () const = 0;
	/** @return length (in source video frames), according to our content's header */
	virtual SourceFrame length () const = 0;

	virtual int time_base_numerator () const = 0;
	virtual int time_base_denominator () const = 0;
	virtual int sample_aspect_ratio_numerator () const = 0;
	virtual int sample_aspect_ratio_denominator () const = 0;

	virtual void set_subtitle_stream (boost::shared_ptr<SubtitleStream>);

	void set_progress () const;
	
	SourceFrame video_frame () const {
		return _video_frame;
	}

	boost::shared_ptr<SubtitleStream> subtitle_stream () const {
		return _subtitle_stream;
	}

	std::vector<boost::shared_ptr<SubtitleStream> > subtitle_streams () const {
		return _subtitle_streams;
	}

	SourceFrame last_source_frame () const {
		return _last_source_frame;
	}

protected:
	
	virtual PixelFormat pixel_format () const = 0;

	void emit_video (boost::shared_ptr<Image>, SourceFrame);
	void emit_subtitle (boost::shared_ptr<TimedSubtitle>);
	void repeat_last_video ();

	/** Subtitle stream to use when decoding */
	boost::shared_ptr<SubtitleStream> _subtitle_stream;
	/** Subtitle streams that this decoder's content has */
	std::vector<boost::shared_ptr<SubtitleStream> > _subtitle_streams;

private:
	void signal_video (boost::shared_ptr<Image>, boost::shared_ptr<Subtitle>);

	SourceFrame _video_frame;
	SourceFrame _last_source_frame;
	
	boost::shared_ptr<TimedSubtitle> _timed_subtitle;

	boost::shared_ptr<Image> _last_image;
	boost::shared_ptr<Subtitle> _last_subtitle;
};

#endif
