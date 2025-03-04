/*
    Copyright (C) 2013-2021 Carl Hetherington <cth@carlh.net>

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


#include "change_signaller.h"
#include "dcpomatic_time.h"
#include "path_behaviour.h"
#include "signaller.h"
#include "user_property.h"
#include "text_type.h"
#include <libcxml/cxml.h>
#include <boost/filesystem.hpp>
#include <boost/signals2.hpp>
#include <boost/thread/mutex.hpp>


namespace xmlpp {
	class Node;
}

namespace cxml {
	class Node;
}

class Job;
class Film;
class AtmosContent;
class AudioContent;
class TextContent;
class VideoContent;

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
class Content : public std::enable_shared_from_this<Content>, public Signaller
{
public:
	Content() = default;
	explicit Content(dcpomatic::DCPTime);
	explicit Content(boost::filesystem::path);
	Content(cxml::ConstNodePtr, boost::optional<boost::filesystem::path> directory);
	explicit Content(std::vector<std::shared_ptr<Content>>);
	virtual ~Content() {}

	Content(Content const&) = delete;
	Content& operator=(Content const&) = delete;

	/** Examine the content to establish digest, frame rates and any other
	 *  useful metadata.
	 *  @param job Job to use to report progress, or 0.
	 *  @param tolerant true to try to carry on in the presence of problems with the content,
	 *  false to throw exceptions in these cases.
	 */
	virtual void examine(std::shared_ptr<const Film> film, std::shared_ptr<Job> job, bool tolerant);

	virtual void take_settings_from(std::shared_ptr<const Content> c);

	/** @return Quick one-line summary of the content, as will be presented in the
	 *  film editor.
	 */
	virtual std::string summary() const = 0;

	/** @return Technical details of this content; these are written to logs to
	 *  help with debugging.
	 */
	virtual std::string technical_summary() const;

	virtual void as_xml(
		xmlpp::Element* element,
		bool with_paths,
		PathBehaviour path_behaviour,
		boost::optional<boost::filesystem::path> film_directory
		) const;

	virtual dcpomatic::DCPTime full_length(std::shared_ptr<const Film>) const = 0;
	virtual dcpomatic::DCPTime approximate_length() const = 0;
	virtual std::string identifier() const;
	/** @return points at which to split this content when
	 *  REELTYPE_BY_VIDEO_CONTENT is in use.
	 */
	virtual std::list<dcpomatic::DCPTime> reel_split_points(std::shared_ptr<const Film>) const;

	std::shared_ptr<Content> clone() const;

	void set_paths(std::vector<boost::filesystem::path> paths);

	std::string path_summary() const;

	std::vector<boost::filesystem::path> paths() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _paths;
	}

	size_t number_of_paths() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _paths.size();
	}

	boost::filesystem::path path(size_t i) const {
		boost::mutex::scoped_lock lm(_mutex);
		return _paths[i];
	}

	std::time_t last_write_time(size_t i) const {
		boost::mutex::scoped_lock lm(_mutex);
		return _last_write_times[i];
	}

	/** @return Digest of the content's file(s).  This is a MD5 digest
	 *  of the first million bytes, the last million bytes, and the
	 *  size of the first file in ASCII.
	 */
	std::string digest() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _digest;
	}

	void set_position(std::shared_ptr<const Film> film, dcpomatic::DCPTime, bool force_emit = false);

	/** dcpomatic::DCPTime that this content starts; i.e. the time that the first
	 *  bit of the content (trimmed or not) will happen.
	 */
	dcpomatic::DCPTime position() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _position;
	}

	void set_trim_start(std::shared_ptr<const Film> film, dcpomatic::ContentTime);

	dcpomatic::ContentTime trim_start() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _trim_start;
	}

	void set_trim_end(dcpomatic::ContentTime);

	dcpomatic::ContentTime trim_end() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _trim_end;
	}

	/** @return Time immediately after the last thing in this content */
	dcpomatic::DCPTime end(std::shared_ptr<const Film> film) const {
		return position() + length_after_trim(film);
	}

	dcpomatic::DCPTimePeriod period(std::shared_ptr<const Film> film) const {
		return { position(), end(film) };
	}

	dcpomatic::DCPTime length_after_trim(std::shared_ptr<const Film> film) const;

	boost::optional<double> video_frame_rate() const {
		boost::mutex::scoped_lock lm(_mutex);
		return _video_frame_rate;
	}

	void set_video_frame_rate(std::shared_ptr<const Film> film, double r);
	void unset_video_frame_rate();

	double active_video_frame_rate(std::shared_ptr<const Film> film) const;

	void set_change_signals_frequent(bool f) {
		_change_signals_frequent = f;
	}

	std::list<UserProperty> user_properties(std::shared_ptr<const Film> film) const;

	std::string calculate_digest() const;

	virtual bool can_be_played() const {
		return true;
	}

	bool has_mapped_audio() const;

	/* ChangeType::PENDING and ChangeType::CANCELLED may be emitted from any thread; ChangeType::DONE always from GUI thread */
	boost::signals2::signal<void (ChangeType, std::weak_ptr<Content>, int, bool)> Change;

	std::shared_ptr<VideoContent> video;
	std::shared_ptr<AudioContent> audio;
	std::vector<std::shared_ptr<TextContent>> text;
	std::shared_ptr<AtmosContent> atmos;

	std::shared_ptr<TextContent> only_text() const;
	std::shared_ptr<TextContent> text_of_original_type(TextType type) const;

	/** @return true if this content has changed since it was last examined */
	bool changed() const;

protected:

	virtual void add_properties(std::shared_ptr<const Film> film, std::list<UserProperty> &) const;

	/** _mutex which should be used to protect accesses, as examine
	 *  jobs can update content state in threads other than the main one.
	 */
	mutable boost::mutex _mutex;

	void add_path(boost::filesystem::path p);

private:
	friend struct ffmpeg_pts_offset_test;
	friend struct best_dcp_frame_rate_test_single;
	friend struct best_dcp_frame_rate_test_double;
	friend struct audio_sampling_rate_test;
	friend struct subtitle_font_id_change_test2;
	template<class, class> friend class ChangeSignalDespatcher;

	void signal_change(ChangeType, int);

	/** Paths of our data files */
	std::vector<boost::filesystem::path> _paths;
	std::vector<std::time_t> _last_write_times;

	std::string _digest;
	dcpomatic::DCPTime _position;
	dcpomatic::ContentTime _trim_start;
	dcpomatic::ContentTime _trim_end;
	/** The video frame rate that this content is or was prepared to be used with,
	 *  or empty if the effective rate of this content should be dictated by something
	 *  else (either some video happening at the same time, or the rate of the DCP).
	 */
	boost::optional<double> _video_frame_rate;
	bool _change_signals_frequent = false;
};


typedef ChangeSignaller<Content, int> ContentChangeSignaller;
typedef ChangeSignalDespatcher<Content, int> ContentChangeSignalDespatcher;


#endif
