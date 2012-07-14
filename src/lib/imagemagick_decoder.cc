#include <iostream>
#include <Magick++/Image.h>
#include "imagemagick_decoder.h"
#include "film_state.h"
#include "image.h"

using namespace std;

ImageMagickDecoder::ImageMagickDecoder (
	boost::shared_ptr<const FilmState> s, boost::shared_ptr<const Options> o, Job* j, Log* l, bool minimal, bool ignore_length)
	: Decoder (s, o, j, l, minimal, ignore_length)
	, _done (false)
{
	_magick_image = new Magick::Image (_fs->content_path ());
}

Size
ImageMagickDecoder::native_size () const
{
	return Size (_magick_image->columns(), _magick_image->rows());
}

bool
ImageMagickDecoder::do_pass ()
{
	if (_done) {
		return true;
	}

	Size size = native_size ();
	RGBFrameImage image (size);

	uint8_t* p = image.data()[0];
	for (int y = 0; y < size.height; ++y) {
		for (int x = 0; x < size.width; ++x) {
			Magick::Color c = _magick_image->pixelColor (x, y);
			*p++ = c.redQuantum() * 255 / MaxRGB;
			*p++ = c.greenQuantum() * 255 / MaxRGB;
			*p++ = c.blueQuantum() * 255 / MaxRGB;
		}

	}
	
	process_video (image.frame ());

	_done = true;
	return false;
}

PixelFormat
ImageMagickDecoder::pixel_format () const
{
	return PIX_FMT_RGB24;
}

