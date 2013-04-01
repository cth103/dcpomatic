#ifndef DVDOMATIC_VIDEO_CONTENT_H
#define DVDOMATIC_VIDEO_CONTENT_H

#include "content.h"
#include "util.h"

class VideoDecoder;

class VideoContentProperty
{
public:
	static int const VIDEO_LENGTH;
	static int const VIDEO_SIZE;
	static int const VIDEO_FRAME_RATE;
};

class VideoContent : public virtual Content
{
public:
	VideoContent (boost::filesystem::path);
	VideoContent (boost::shared_ptr<const cxml::Node>);

	void as_xml (xmlpp::Node *) const;

	ContentVideoFrame video_length () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _video_length;
	}

	libdcp::Size video_size () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _video_size;
	}
	
	float video_frame_rate () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _video_frame_rate;
	}

protected:
	void take_from_video_decoder (boost::shared_ptr<VideoDecoder>);

	ContentVideoFrame _video_length;

private:
	libdcp::Size _video_size;
	float _video_frame_rate;
};

#endif
