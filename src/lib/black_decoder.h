#include "video_decoder.h"

class BlackDecoder : public VideoDecoder
{
public:
	BlackDecoder (boost::shared_ptr<NullContent>);

	bool pass ();
	bool seek (double);
	Time next () const;

	/** @return video frame rate second, or 0 if unknown */
	float video_frame_rate () const {
		return 24;
	}
	
	/** @return native size in pixels */
	libdcp::Size native_size () const {
		return libdcp::Size (256, 256);
	}
	
	/** @return length according to our content's header */
	ContentVideoFrame video_length () const {
		return _content_length;
	}

protected:	

	int time_base_numerator () const {
		return 0;
	}
	
	int time_base_denominator () const {
		return 1;
	}
	
	int sample_aspect_ratio_numerator () const {
		return 0;
	}
		
	int sample_aspect_ratio_denominator () const {
		return 1;
	}
};
