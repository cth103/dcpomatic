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
		/** a normal frame with some JPEG200 data */
		FULL,
		/** a frame whose data already exists in the MXF,
		    and we fake-write it; i.e. we update the writer's
		    state but we use the data that is already on disk.
		*/
		FAKE,
		/** this is a repeat of the last frame to be written */
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

	/** our Film */
	boost::shared_ptr<Film> _film;
	/** the first frame index that does not already exist in our MXF */
	int _first_nonexistant_frame;

	/** our thread, or 0 */
	boost::thread* _thread;
	/** true if our thread should finish */
	bool _finish;
	/** queue of things to write to disk */
	std::list<QueueItem> _queue;
	/** number of FULL frames whose JPEG200 data is currently held in RAM */
	int _queued_full_in_memory;
	/** mutex for thread state */
	mutable boost::mutex _mutex;
	/** condition to manage thread wakeups */
	boost::condition _condition;
	/** the data of the last written frame, or 0 if there isn't one */
	boost::shared_ptr<const EncodedData> _last_written;
	/** the index of the last written frame */
	int _last_written_frame;
	/** maximum number of frames to hold in memory, for when we are managing
	    ordering
	*/
	static const int _maximum_frames_in_memory;

	/** number of FULL written frames */
	int _full_written;
	/** number of FAKE written frames */
	int _fake_written;
	/** number of REPEAT written frames */
	int _repeat_written;
	/** number of frames pushed to disk and then recovered
	    due to the limit of frames to be held in memory.
	*/
	int _pushed_to_disk;

	boost::shared_ptr<libdcp::MonoPictureAsset> _picture_asset;
	boost::shared_ptr<libdcp::MonoPictureAssetWriter> _picture_asset_writer;
	boost::shared_ptr<libdcp::SoundAsset> _sound_asset;
	boost::shared_ptr<libdcp::SoundAssetWriter> _sound_asset_writer;
};
