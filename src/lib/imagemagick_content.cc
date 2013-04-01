#include <libcxml/cxml.h>
#include "imagemagick_content.h"
#include "compose.hpp"

#include "i18n.h"

using std::string;
using boost::shared_ptr;

ImageMagickContent::ImageMagickContent (boost::filesystem::path f)
	: Content (f)
	, VideoContent (f)
{
	/* XXX */
	_video_length = 10 * 24;
}

ImageMagickContent::ImageMagickContent (shared_ptr<const cxml::Node> node)
	: Content (node)
	, VideoContent (node)
{
	
}

string
ImageMagickContent::summary () const
{
	return String::compose (_("Image: %1"), file().filename ());
}

bool
ImageMagickContent::valid_file (boost::filesystem::path f)
{
	string ext = f.extension().string();
	transform (ext.begin(), ext.end(), ext.begin(), ::tolower);
	return (ext == ".tif" || ext == ".tiff" || ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp");
}
