#ifndef DVDOMATIC_VIDEO_DECODER_H
#define DVDOMATIC_VIDEO_DECODER_H

#include "video_source.h"
#include "stream.h"
#include "decoder.h"

class VideoDecoder : public VideoSource, public virtual Decoder
{
public:
	VideoDecoder (boost::shared_ptr<Film>, boost::shared_ptr<const Options>, Job *);

	/** @return video frames per second, or 0 if unknown */
	virtual float frames_per_second () const = 0;
	/** @return native size in pixels */
	virtual Size native_size () const = 0;

	virtual int time_base_numerator () const = 0;
	virtual int time_base_denominator () const = 0;
	virtual int sample_aspect_ratio_numerator () const = 0;
	virtual int sample_aspect_ratio_denominator () const = 0;

	virtual void set_subtitle_stream (boost::optional<SubtitleStream>);

	SourceFrame video_frame () const {
		return _video_frame;
	}

	boost::optional<SubtitleStream> subtitle_stream () const {
		return _subtitle_stream;
	}

	std::vector<SubtitleStream> subtitle_streams () const {
		return _subtitle_streams;
	}

protected:
	
	virtual PixelFormat pixel_format () const = 0;
	void set_progress () const;

	void emit_video (boost::shared_ptr<Image>);
	void emit_subtitle (boost::shared_ptr<TimedSubtitle>);
	void repeat_last_video ();

	boost::optional<SubtitleStream> _subtitle_stream;
	std::vector<SubtitleStream> _subtitle_streams;

private:
	void signal_video (boost::shared_ptr<Image>, boost::shared_ptr<Subtitle>);

	SourceFrame _video_frame;

	boost::shared_ptr<TimedSubtitle> _timed_subtitle;

	boost::shared_ptr<Image> _last_image;
	boost::shared_ptr<Subtitle> _last_subtitle;
};

#endif
