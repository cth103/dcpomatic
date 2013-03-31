#include "video_content.h"
#include "video_decoder.h"

int const VideoContentProperty::VIDEO_LENGTH = 0;
int const VideoContentProperty::VIDEO_SIZE = 1;
int const VideoContentProperty::VIDEO_FRAME_RATE = 2;

using boost::shared_ptr;

VideoContent::VideoContent (boost::filesystem::path f)
	: Content (f)
	, _video_length (0)
{

}

void
VideoContent::take_from_video_decoder (shared_ptr<VideoDecoder> d)
{
        {
                boost::mutex::scoped_lock lm (_mutex);
                _video_size = d->native_size ();
                _video_frame_rate = d->frames_per_second ();
        }
        
        Changed (VideoContentProperty::VIDEO_SIZE);
        Changed (VideoContentProperty::VIDEO_FRAME_RATE);
}
