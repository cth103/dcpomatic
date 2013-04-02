#include <libcxml/cxml.h>
#include "video_content.h"
#include "video_decoder.h"

#include "i18n.h"

int const VideoContentProperty::VIDEO_LENGTH = 0;
int const VideoContentProperty::VIDEO_SIZE = 1;
int const VideoContentProperty::VIDEO_FRAME_RATE = 2;

using std::string;
using std::stringstream;
using std::setprecision;
using boost::shared_ptr;
using boost::lexical_cast;

VideoContent::VideoContent (boost::filesystem::path f)
	: Content (f)
	, _video_length (0)
{

}

VideoContent::VideoContent (shared_ptr<const cxml::Node> node)
	: Content (node)
{
	_video_length = node->number_child<ContentVideoFrame> ("VideoLength");
	_video_size.width = node->number_child<int> ("VideoWidth");
	_video_size.height = node->number_child<int> ("VideoHeight");
	_video_frame_rate = node->number_child<float> ("VideoFrameRate");
}

VideoContent::VideoContent (VideoContent const & o)
	: Content (o)
	, _video_length (o._video_length)
	, _video_size (o._video_size)
	, _video_frame_rate (o._video_frame_rate)
{

}

void
VideoContent::as_xml (xmlpp::Node* node) const
{
	boost::mutex::scoped_lock lm (_mutex);
	node->add_child("VideoLength")->add_child_text (lexical_cast<string> (_video_length));
	node->add_child("VideoWidth")->add_child_text (lexical_cast<string> (_video_size.width));
	node->add_child("VideoHeight")->add_child_text (lexical_cast<string> (_video_size.height));
	node->add_child("VideoFrameRate")->add_child_text (lexical_cast<string> (_video_frame_rate));
}

void
VideoContent::take_from_video_decoder (shared_ptr<VideoDecoder> d)
{
	/* These decoder calls could call other content methods which take a lock on the mutex */
	libdcp::Size const vs = d->native_size ();
	float const vfr = d->frames_per_second ();
	
        {
                boost::mutex::scoped_lock lm (_mutex);
                _video_size = vs;
		_video_frame_rate = vfr;
        }
        
        Changed (VideoContentProperty::VIDEO_SIZE);
        Changed (VideoContentProperty::VIDEO_FRAME_RATE);
}


string
VideoContent::information () const
{
	stringstream s;

	s << String::compose (
		_("%1x%2 pixels (%3:1)"),
		video_size().width,
		video_size().height,
		setprecision (3), float (video_size().width) / video_size().height
		);
	
	return s.str ();
}
