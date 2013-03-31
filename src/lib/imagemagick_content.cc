#include "imagemagick_content.h"
#include "compose.hpp"

#include "i18n.h"

using std::string;

ImageMagickContent::ImageMagickContent (boost::filesystem::path f)
	: Content (f)
	, VideoContent (f)
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
