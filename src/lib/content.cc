#include <boost/thread/mutex.hpp>
#include <libxml++/libxml++.h>
#include <libcxml/cxml.h>
#include "content.h"
#include "util.h"

using std::string;
using boost::shared_ptr;

Content::Content (boost::filesystem::path f)
	: _file (f)
{

}

Content::Content (shared_ptr<const cxml::Node> node)
{
	_file = node->string_child ("File");
	_digest = node->string_child ("Digest");
}

void
Content::as_xml (xmlpp::Node* node) const
{
	boost::mutex::scoped_lock lm (_mutex);
	node->add_child("File")->add_child_text (_file.string());
	node->add_child("Digest")->add_child_text (_digest);
}

void
Content::examine (shared_ptr<Film>, shared_ptr<Job>, bool)
{
	string const d = md5_digest (_file);
	boost::mutex::scoped_lock lm (_mutex);
	_digest = d;
}
