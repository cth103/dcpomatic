#ifndef DVDOMATIC_VIDEO_SOURCE_H
#define DVDOMATIC_VIDEO_SOURCE_H

#include <boost/shared_ptr.hpp>
#include <boost/signals2.hpp>
#include "util.h"

class VideoSink;
class Subtitle;
class Image;

class VideoSource
{
public:

	/** Emitted when a video frame is ready.
	 *  First parameter is the frame within the source.
	 *  Second parameter is either 0 or a subtitle that should be on this frame.
	 */
	boost::signals2::signal<void (boost::shared_ptr<Image>, boost::shared_ptr<Subtitle>)> Video;

	void connect_video (boost::shared_ptr<VideoSink>);
};

#endif
