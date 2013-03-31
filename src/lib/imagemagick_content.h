#include "video_content.h"

class ImageMagickContent : public VideoContent
{
public:
	ImageMagickContent (boost::filesystem::path);
};
