#include "decoder.h"

namespace Magick {
	class Image;
}

class ImageMagickDecoder : public Decoder
{
public:
	ImageMagickDecoder (boost::shared_ptr<const FilmState>, boost::shared_ptr<const Options>, Job *, Log *, bool, bool);

	int length_in_frames () const {
		return 1;
	}

	float frames_per_second () const {
		return static_frames_per_second ();
	}

	Size native_size () const;

	int audio_channels () const {
		return 0;
	}

	int audio_sample_rate () const {
		return 0;
	}

	AVSampleFormat audio_sample_format () const {
		return AV_SAMPLE_FMT_NONE;
	}

	static float static_frames_per_second () {
		return 24;
	}

protected:
	bool do_pass ();
	PixelFormat pixel_format () const;

	int time_base_numerator () const {
		return 0;
	}

	int time_base_denominator () const {
		return 0;
	}

	int sample_aspect_ratio_numerator () const {
		/* XXX */
		return 1;
	}

	int sample_aspect_ratio_denominator () const {
		/* XXX */
		return 1;
	}

private:
	Magick::Image* _magick_image;
	bool _done;
};
