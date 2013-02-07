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
class AudioBuffers;

namespace libdcp {
	class MonoPictureAsset;
	class MonoPictureAssetWriter;
	class SoundAsset;
	class SoundAssetWriter;
}

struct QueueItem
{
public:
	enum Type {
		FULL,
		FAKE,
		REPEAT
	} type;

	/** encoded data for FULL */
	boost::shared_ptr<const EncodedData> encoded;
	/** size of data for FAKE */
	int size;
	/** frame index */
	int frame;
};

bool operator< (QueueItem const & a, QueueItem const & b);
bool operator== (QueueItem const & a, QueueItem const & b);

class Writer
{
public:
	Writer (boost::shared_ptr<Film>);

	bool can_fake_write (int) const;
	
	void write (boost::shared_ptr<const EncodedData>, int);
	void fake_write (int);
	void write (boost::shared_ptr<const AudioBuffers>);
	void repeat (int f);
	void finish ();

private:

	void thread ();
	void check_existing_picture_mxf ();

	boost::shared_ptr<Film> _film;
	int _first_nonexistant_frame;

	boost::thread* _thread;
	bool _finish;
	std::list<QueueItem> _queue;
	int _queued_full_in_memory;
	mutable boost::mutex _mutex;
	boost::condition _condition;
	boost::shared_ptr<const EncodedData> _last_written;
	int _last_written_frame;
	static const int _maximum_frames_in_memory;

	boost::shared_ptr<libdcp::MonoPictureAsset> _picture_asset;
	boost::shared_ptr<libdcp::MonoPictureAssetWriter> _picture_asset_writer;
	boost::shared_ptr<libdcp::SoundAsset> _sound_asset;
	boost::shared_ptr<libdcp::SoundAssetWriter> _sound_asset_writer;
};
