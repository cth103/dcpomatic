#include <boost/filesystem.hpp>
#include <boost/thread/mutex.hpp>

class Content
{
public:
	boost::filesystem::path file () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _file;
	}

protected:
	boost::mutex _mutex;

private:
	boost::filesystem::path _file;
};
