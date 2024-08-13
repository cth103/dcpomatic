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


#ifndef DCPOMATIC_PLAYLIST_H
#define DCPOMATIC_PLAYLIST_H


#include "change_signaller.h"
#include "dcpomatic_time.h"
#include "frame_rate_change.h"
#include "path_behaviour.h"
#include "types.h"
#include <libcxml/cxml.h>
#include <boost/signals2.hpp>
#include <boost/thread.hpp>
#include <list>


class Film;


/** @class Playlist
 *  @brief A set of Content objects with knowledge of how they should be arranged into
 *  a DCP.
 */
class Playlist
{
public:
	Playlist() = default;

	~Playlist ();

	Playlist (Playlist const&) = delete;
	Playlist& operator= (Playlist const&) = delete;

	void as_xml(xmlpp::Element*, bool with_content_paths, PathBehaviour path_behaviour, boost::optional<boost::filesystem::path> film_directory);
	void set_from_xml (std::shared_ptr<const Film> film, cxml::ConstNodePtr node, int version, std::list<std::string>& notes);

	void add (std::shared_ptr<const Film> film, std::shared_ptr<Content>);
	void remove (std::shared_ptr<Content>);
	void remove (ContentList);
	void move_earlier (std::shared_ptr<const Film> film, std::shared_ptr<Content>);
	void move_later (std::shared_ptr<const Film> film, std::shared_ptr<Content>);

	ContentList content () const;

	std::string video_identifier () const;

	dcpomatic::DCPTime length (std::shared_ptr<const Film> film) const;
	boost::optional<dcpomatic::DCPTime> start () const;
	int64_t required_disk_space(std::shared_ptr<const Film> film, int64_t video_bit_rate, int audio_channels, int audio_frame_rate) const;

	int best_video_frame_rate () const;
	dcpomatic::DCPTime video_end (std::shared_ptr<const Film> film) const;
	dcpomatic::DCPTime text_end (std::shared_ptr<const Film> film) const;
	FrameRateChange active_frame_rate_change (dcpomatic::DCPTime, int dcp_frame_rate) const;
	std::string content_summary (std::shared_ptr<const Film> film, dcpomatic::DCPTimePeriod period) const;
	std::pair<double, double> speed_up_range (int dcp_video_frame_rate) const;

	void set_sequence (bool);
	void maybe_sequence (std::shared_ptr<const Film> film);

	void repeat (std::shared_ptr<const Film> film, ContentList, int);

	/** Emitted when content has been added to or removed from the playlist; implies OrderChange */
	mutable boost::signals2::signal<void (ChangeType)> Change;
	mutable boost::signals2::signal<void ()> OrderChange;
	/** Emitted when the length might have changed; may sometimes be emitted when it has not */
	mutable boost::signals2::signal<void ()> LengthChange;

	mutable boost::signals2::signal<void (ChangeType, std::weak_ptr<Content>, int, bool)> ContentChange;

private:
	void content_change (std::weak_ptr<const Film>, ChangeType, std::weak_ptr<Content>, int, bool);
	void disconnect ();
	void reconnect (std::shared_ptr<const Film> film);

	mutable boost::mutex _mutex;
	/** List of content, kept sorted by ContentSorter() */
	ContentList _content;
	bool _sequence = true;
	bool _sequencing = false;
	std::list<boost::signals2::connection> _content_connections;
};


#endif
