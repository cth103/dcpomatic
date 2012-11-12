#ifndef DVDOMATIC_VIDEO_SINK_H
#define DVDOMATIC_VIDEO_SINK_H

#include <boost/shared_ptr.hpp>
#include "util.h"

class Subtitle;
class Image;

class VideoSink
{
public:
	/** Call with a frame of video.
	 *  @param i Video frame image.
	 *  @param s A subtitle that should be on this frame, or 0.
	 */
	virtual void process_video (boost::shared_ptr<Image> i, boost::shared_ptr<Subtitle> s) = 0;
};

#endif
