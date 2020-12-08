/*
    Copyright (C) 2012-2020 Carl Hetherington <cth@carlh.net>

    This file is part of DCP-o-matic.

    DCP-o-matic is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DCP-o-matic is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DCP-o-matic.  If not, see <http://www.gnu.org/licenses/>.

*/

/** @file  src/lib/writer.h
 *  @brief Writer class.
 */

#include "atmos_metadata.h"
#include "types.h"
#include "player_text.h"
#include "exception_store.h"
#include "dcp_text_track.h"
#include "weak_film.h"
#include <dcp/atmos_frame.h>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/thread/condition.hpp>
#include <list>

namespace dcp {
	class Data;
}

namespace dcpomatic {
	class Font;
}

class Film;
class AudioBuffers;
class Job;
class ReferencedReelAsset;
class ReelWriter;

struct QueueItem
{
public:
	QueueItem ()
		: size (0)
		, reel (0)
		, frame (0)
		, eyes (EYES_BOTH)
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
	boost::shared_ptr<const dcp::Data> encoded;
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
 *  write() for Data (picture) can be called out of order, and the Writer
 *  will sort it out.  write() for AudioBuffers must be called in order.
 */

class Writer : public ExceptionStore, public boost::noncopyable, public WeakConstFilm
{
public:
	Writer (boost::weak_ptr<const Film>, boost::weak_ptr<Job>);
	~Writer ();

	void start ();

	bool can_fake_write (Frame) const;

	void write (boost::shared_ptr<const dcp::Data>, Frame, Eyes);
	void fake_write (Frame, Eyes);
	bool can_repeat (Frame) const;
	void repeat (Frame, Eyes);
	void write (boost::shared_ptr<const AudioBuffers>, dcpomatic::DCPTime time);
	void write (PlayerText text, TextType type, boost::optional<DCPTextTrack>, dcpomatic::DCPTimePeriod period);
	void write (std::list<boost::shared_ptr<dcpomatic::Font> > fonts);
	void write (ReferencedReelAsset asset);
	void write (boost::shared_ptr<const dcp::AtmosFrame> atmos, dcpomatic::DCPTime time, AtmosMetadata metadata);
	void finish (boost::filesystem::path output_dcp);

	void set_encoder_threads (int threads);

private:
	void thread ();
	void terminate_thread (bool);
	bool have_sequenced_image_at_queue_head ();
	size_t video_reel (int frame) const;
	void set_digest_progress (Job* job, float progress);
	void write_cover_sheet (boost::filesystem::path output_dcp);
	void calculate_referenced_digests (boost::function<void (float)> set_progress);

	boost::weak_ptr<Job> _job;
	std::vector<ReelWriter> _reels;
	std::vector<ReelWriter>::iterator _audio_reel;
	std::vector<ReelWriter>::iterator _subtitle_reel;
	std::map<DCPTextTrack, std::vector<ReelWriter>::iterator> _caption_reels;
	std::vector<ReelWriter>::iterator _atmos_reel;

	/** our thread */
	boost::thread _thread;
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
	unsigned int _maximum_queue_size;

	class LastWritten
	{
	public:
		LastWritten()
			: _frame(-1)
			, _eyes(EYES_RIGHT)
		{}

		/** @return true if qi is the next item after this one */
		bool next (QueueItem qi) const;
		void update (QueueItem qi);

		int frame () const {
			return _frame;
		}

	private:
		int _frame;
		Eyes _eyes;
	};

	/** The last frame written to each reel */
	std::vector<LastWritten> _last_written;

	/** number of FULL written frames */
	int _full_written;
	/** number of FAKE written frames */
	int _fake_written;
	int _repeat_written;
	/** number of frames pushed to disk and then recovered
	    due to the limit of frames to be held in memory.
	*/
	int _pushed_to_disk;

	boost::mutex _digest_progresses_mutex;
	std::map<boost::thread::id, float> _digest_progresses;

	std::list<ReferencedReelAsset> _reel_assets;

	std::list<boost::shared_ptr<dcpomatic::Font> > _fonts;
};
