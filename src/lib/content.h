/*
    Copyright (C) 2013-2018 Carl Hetherington <cth@carlh.net>

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

/** @file  src/lib/content.h
 *  @brief Content class.
 */

#ifndef DCPOMATIC_CONTENT_H
#define DCPOMATIC_CONTENT_H

#include "types.h"
#include "signaller.h"
#include "dcpomatic_time.h"
#include "change_signaller.h"
#include "user_property.h"
#include <libcxml/cxml.h>
#include <boost/filesystem.hpp>
#include <boost/signals2.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/enable_shared_from_this.hpp>

namespace xmlpp {
	class Node;
}

namespace cxml {
	class Node;
}

class Job;
class Film;

class ContentProperty
{
public:
	static int const PATH;
	static int const POSITION;
	static int const LENGTH;
	static int const TRIM_START;
	static int const TRIM_END;
	static int const VIDEO_FRAME_RATE;
};

/** @class Content
 *  @brief A piece of content represented by one or more files on disk.
 */
class Content : public boost::enable_shared_from_this<Content>, public Signaller, public boost::noncopyable
{
public:
	explicit Content ();
	Content (DCPTime);
	Content (boost::filesystem::path);
	Content (cxml::ConstNodePtr);
	Content (std::vector<boost::shared_ptr<Content> >);
	virtual ~Content () {}

	/** Examine the content to establish digest, frame rates and any other
	 *  useful metadata.
	 *  @param job Job to use to report progress, or 0.
	 */
	virtual void examine (boost::shared_ptr<const Film> film, boost::shared_ptr<Job> job);

	virtual void take_settings_from (boost::shared_ptr<const Content> c);

	/** @return Quick one-line summary of the content, as will be presented in the
	 *  film editor.
	 */
	virtual std::string summary () const = 0;

	/** @return Technical details of this content; these are written to logs to
	 *  help with debugging.
	 */
	virtual std::string technical_summary () const;

	virtual void as_xml (xmlpp::Node *, bool with_paths) const;
	virtual DCPTime full_length (boost::shared_ptr<const Film>) const = 0;
	virtual std::string identifier () const;
	/** @return points at which to split this content when
	 *  REELTYPE_BY_VIDEO_CONTENT is in use.
	 */
	virtual std::list<DCPTime> reel_split_points () const;

	boost::shared_ptr<Content> clone (boost::shared_ptr<const Film> film) const;

	void set_paths (std::vector<boost::filesystem::path> paths);

	std::string path_summary () const;

	std::vector<boost::filesystem::path> paths () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _paths;
	}

	size_t number_of_paths () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _paths.size ();
	}

	boost::filesystem::path path (size_t i) const {
		boost::mutex::scoped_lock lm (_mutex);
		return _paths[i];
	}

	std::time_t last_write_time (size_t i) const {
		boost::mutex::scoped_lock lm (_mutex);
		return _last_write_times[i];
	}

	bool paths_valid () const;

	/** @return Digest of the content's file(s).  Note: this is
	 *  not a complete MD5-or-whatever hash, but a sort of poor
	 *  man's version (see comments in examine()).
	 */
	std::string digest () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _digest;
	}

	void set_position (boost::shared_ptr<const Film> film, DCPTime);

	/** DCPTime that this content starts; i.e. the time that the first
	 *  bit of the content (trimmed or not) will happen.
	 */
	DCPTime position () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _position;
	}

	void set_trim_start (ContentTime);

	ContentTime trim_start () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _trim_start;
	}

	void set_trim_end (ContentTime);

	ContentTime trim_end () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _trim_end;
	}

	/** @return Time immediately after the last thing in this content */
	DCPTime end (boost::shared_ptr<const Film> film) const {
		return position() + length_after_trim(film);
	}

	DCPTime length_after_trim (boost::shared_ptr<const Film> film) const;

	boost::optional<double> video_frame_rate () const {
		boost::mutex::scoped_lock lm (_mutex);
		return _video_frame_rate;
	}

	void set_video_frame_rate (double r);
	void unset_video_frame_rate ();

	double active_video_frame_rate (boost::shared_ptr<const Film> film) const;

	void set_change_signals_frequent (bool f) {
		_change_signals_frequent = f;
	}

	std::list<UserProperty> user_properties () const;

	std::string calculate_digest () const;

	/* CHANGE_PENDING and CHANGE_CANCELLED may be emitted from any thread; CHANGE_DONE always from GUI thread */
	boost::signals2::signal<void (ChangeType, boost::weak_ptr<Content>, int, bool)> Change;

	boost::shared_ptr<VideoContent> video;
	boost::shared_ptr<AudioContent> audio;
	std::list<boost::shared_ptr<TextContent> > text;

	boost::shared_ptr<TextContent> only_text () const;
	boost::shared_ptr<TextContent> text_of_original_type (TextType type) const;

protected:

	virtual void add_properties (std::list<UserProperty> &) const;

	/** _mutex which should be used to protect accesses, as examine
	 *  jobs can update content state in threads other than the main one.
	 */
	mutable boost::mutex _mutex;

	void add_path (boost::filesystem::path p);

private:
	friend struct ffmpeg_pts_offset_test;
	friend struct best_dcp_frame_rate_test_single;
	friend struct best_dcp_frame_rate_test_double;
	friend struct audio_sampling_rate_test;
	template<class> friend class ChangeSignaller;

	void signal_change (ChangeType, int);

	/** Paths of our data files */
	std::vector<boost::filesystem::path> _paths;
	std::vector<std::time_t> _last_write_times;

	std::string _digest;
	DCPTime _position;
	ContentTime _trim_start;
	ContentTime _trim_end;
	/** The video frame rate that this content is or was prepared to be used with,
	 *  or empty if the effective rate of this content should be dictated by something
	 *  else (either some video happening at the same time, or the rate of the DCP).
	 */
	boost::optional<double> _video_frame_rate;
	bool _change_signals_frequent;
};

#endif
