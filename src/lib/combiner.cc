#include "combiner.h"
#include "image.h"

using boost::shared_ptr;

Combiner::Combiner (Log* log)
	: VideoProcessor (log)
{

}

void
Combiner::process_video (shared_ptr<Image> image, shared_ptr<Subtitle> sub)
{
	_image = image;
}

void
Combiner::process_video_b (shared_ptr<Image> image, shared_ptr<Subtitle> sub)
{
	/* Copy the right half of this image into our _image */
	for (int i = 0; i < image->components(); ++i) {
		int const line_size = image->line_size()[i];
		int const half_line_size = line_size / 2;
		int const stride = image->stride()[i];

		uint8_t* p = _image->data()[i];
		uint8_t* q = image->data()[i];
			
		for (int j = 0; j < image->lines (i); ++j) {
			memcpy (p + half_line_size, q + half_line_size, half_line_size);
			p += stride;
			q += stride;
		}
	}

	Video (_image, sub);
	_image.reset ();
}
