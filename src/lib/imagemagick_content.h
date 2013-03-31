#include "video_content.h"

class ImageMagickContent : public VideoContent
{
public:
	ImageMagickContent (boost::filesystem::path);

	std::string summary () const;

	static bool valid_file (boost::filesystem::path);
};
