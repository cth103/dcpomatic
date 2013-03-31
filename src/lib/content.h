#ifndef DVDOMATIC_CONTENT_H
#define DVDOMATIC_CONTENT_H

#include <boost/filesystem.hpp>
#include <boost/signals2.hpp>
#include <boost/thread/mutex.hpp>

class Job;
class Film;

class Content
{
public:
	Content (boost::filesystem::path);
	
	virtual void examine (boost::shared_ptr<Film>, boost::shared_ptr<Job>, bool);
	virtual std::string summary () const = 0;
	
	boost::filesystem::path file () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _file;
	}

	boost::signals2::signal<void (int)> Changed;

protected:
	mutable boost::mutex _mutex;

private:
	boost::filesystem::path _file;
	std::string _digest;
};

#endif
