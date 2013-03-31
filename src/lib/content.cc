#include <boost/thread/mutex.hpp>
#include "content.h"
#include "util.h"

using std::string;
using boost::shared_ptr;

Content::Content (boost::filesystem::path f)
	: _file (f)
{

}

void
Content::examine (shared_ptr<Film>, shared_ptr<Job>, bool)
{
	string const d = md5_digest (_file);
	boost::mutex::scoped_lock lm (_mutex);
	_digest = d;
}
