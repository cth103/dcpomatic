#include "processor.h"

class Combiner : public VideoProcessor
{
public:
	Combiner (Log* log);

	void process_video (boost::shared_ptr<Image> i, boost::shared_ptr<Subtitle> s);
	void process_video_b (boost::shared_ptr<Image> i, boost::shared_ptr<Subtitle> s);

private:
	boost::shared_ptr<Image> _image;
};
