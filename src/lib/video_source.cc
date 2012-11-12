#include "video_source.h"
#include "video_sink.h"

using boost::shared_ptr;
using boost::bind;

void
VideoSource::connect_video (shared_ptr<VideoSink> s)
{
	Video.connect (bind (&VideoSink::process_video, s, _1, _2));
}
