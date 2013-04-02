#include <libcxml/cxml.h>
#include "audio_content.h"

using boost::shared_ptr;

AudioContent::AudioContent (boost::filesystem::path f)
	: Content (f)
{

}

AudioContent::AudioContent (shared_ptr<const cxml::Node> node)
	: Content (node)
{

}

AudioContent::AudioContent (AudioContent const & o)
	: Content (o)
{

}
