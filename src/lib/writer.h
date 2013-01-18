/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <list>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>

class Film;
class EncodedData;

namespace libdcp {
	class MonoPictureAsset;
	class MonoPictureAssetWriter;
}

class Writer
{
public:
	Writer (boost::shared_ptr<const Film>);
	
	void write (boost::shared_ptr<EncodedData>, int);
	void repeat (int f);
	void finish ();

private:

	void thread ();

	boost::shared_ptr<const Film> _film;

	boost::thread* _thread;
	bool _finish;
	std::list<std::pair<boost::shared_ptr<EncodedData>, int> > _queue;
	mutable boost::mutex _mutex;
	boost::condition _condition;
	boost::shared_ptr<EncodedData> _last_written;
	std::list<int> _pending;
	int _last_written_frame;
	static const unsigned int _maximum_frames_in_memory;

	boost::shared_ptr<libdcp::MonoPictureAsset> _picture_asset;
	boost::shared_ptr<libdcp::MonoPictureAssetWriter> _picture_asset_writer;
};
