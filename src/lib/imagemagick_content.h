#include "video_content.h"

namespace cxml {
	class Node;
}

class ImageMagickContent : public VideoContent
{
public:
	ImageMagickContent (boost::filesystem::path);
	ImageMagickContent (boost::shared_ptr<const cxml::Node>);

	std::string summary () const;

	static bool valid_file (boost::filesystem::path);
};
