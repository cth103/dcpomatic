/*
    Copyright (C) 2012-2015 Carl Hetherington <cth@carlh.net>

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

/** @file  src/lib/writer.h
 *  @brief Writer class.
 */

#include "types.h"
#include "player_subtitles.h"
#include "data.h"
#include "exception_store.h"
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <list>

class Film;
class Data;
class AudioBuffers;
class Job;
class Font;
class ReferencedReelAsset;
class ReelWriter;

struct QueueItem
{
public:
	QueueItem ()
		: size (0)
	{}

	enum Type {
		/** a normal frame with some JPEG200 data */
		FULL,
		/** a frame whose data already exists in the MXF,
		    and we fake-write it; i.e. we update the writer's
		    state but we use the data that is already on disk.
		*/
		FAKE,
		REPEAT,
	} type;

	/** encoded data for FULL */
	boost::optional<Data> encoded;
	/** size of data for FAKE */
	int size;
	/** reel index */
	size_t reel;
	/** frame index within the reel */
	int frame;
	/** eyes for FULL, FAKE and REPEAT */
	Eyes eyes;
};

bool operator< (QueueItem const & a, QueueItem const & b);
bool operator== (QueueItem const & a, QueueItem const & b);

/** @class Writer
 *  @brief Class to manage writing JPEG2000 and audio data to assets on disk.
 *
 *  This class creates sound and picture assets, then takes Data
 *  or AudioBuffers objects (containing image or sound data respectively)
 *  and writes them to the assets.
 *
 *  ::write() for Data (picture) can be called out of order, and the Writer
 *  will sort it out.  write() for AudioBuffers must be called in order.
 */

class Writer : public ExceptionStore, public boost::noncopyable
{
public:
	Writer (boost::shared_ptr<const Film>, boost::weak_ptr<Job>);
	~Writer ();

	void start ();

	bool can_fake_write (Frame) const;

	void write (Data, Frame, Eyes);
	void fake_write (Frame, Eyes);
	void repeat (Frame, Eyes);
	void write (boost::shared_ptr<const AudioBuffers>);
	void write (PlayerSubtitles subs);
	void write (std::list<boost::shared_ptr<Font> > fonts);
	void write (ReferencedReelAsset asset);
	void finish ();

	void set_encoder_threads (int threads);

private:
	void thread ();
	void terminate_thread (bool);
	bool have_sequenced_image_at_queue_head ();
	size_t video_reel (int frame) const;

	/** our Film */
	boost::shared_ptr<const Film> _film;
	boost::weak_ptr<Job> _job;
	std::vector<ReelWriter> _reels;
	std::vector<ReelWriter>::iterator _audio_reel;
	std::vector<ReelWriter>::iterator _subtitle_reel;

	/** our thread, or 0 */
	boost::thread* _thread;
	/** true if our thread should finish */
	bool _finish;
	/** queue of things to write to disk */
	std::list<QueueItem> _queue;
	/** number of FULL frames whose JPEG200 data is currently held in RAM */
	int _queued_full_in_memory;
	/** mutex for thread state */
	mutable boost::mutex _state_mutex;
	/** condition to manage thread wakeups when we have nothing to do  */
	boost::condition _empty_condition;
	/** condition to manage thread wakeups when we have too much to do */
	boost::condition _full_condition;
	/** maximum number of frames to hold in memory, for when we are managing
	 *  ordering
	 */
	int _maximum_frames_in_memory;

	/** number of FULL written frames */
	int _full_written;
	/** number of FAKE written frames */
	int _fake_written;
	int _repeat_written;
	/** number of frames pushed to disk and then recovered
	    due to the limit of frames to be held in memory.
	*/
	int _pushed_to_disk;

	std::list<ReferencedReelAsset> _reel_assets;

	std::list<boost::shared_ptr<Font> > _fonts;
};
