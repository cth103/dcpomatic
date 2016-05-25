/*
    Copyright (C) 2013-2016 Carl Hetherington <cth@carlh.net>

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

#include "util.h"
#include "frame_rate_change.h"
#include <libcxml/cxml.h>
#include <boost/shared_ptr.hpp>
#include <boost/signals2.hpp>
#include <list>

class Film;

struct ContentSorter
{
	bool operator() (boost::shared_ptr<Content> a, boost::shared_ptr<Content> b);
};

/** @class Playlist
 *  @brief A set of Content objects with knowledge of how they should be arranged into
 *  a DCP.
 */
class Playlist : public boost::noncopyable
{
public:
	Playlist ();
	~Playlist ();

	void as_xml (xmlpp::Node *);
	void set_from_xml (boost::shared_ptr<const Film>, cxml::ConstNodePtr, int, std::list<std::string> &);

	void add (boost::shared_ptr<Content>);
	void remove (boost::shared_ptr<Content>);
	void remove (ContentList);
	void move_earlier (boost::shared_ptr<Content>);
	void move_later (boost::shared_ptr<Content>);

	ContentList content () const;

	std::string video_identifier () const;

	DCPTime length () const;
	boost::optional<DCPTime> start () const;
	int64_t required_disk_space (int j2k_bandwidth, int audio_channels, int audio_frame_rate) const;

	int best_video_frame_rate () const;
	DCPTime video_end () const;
	DCPTime subtitle_end () const;
	FrameRateChange active_frame_rate_change (DCPTime, int dcp_frame_rate) const;

	void set_sequence (bool);
	void maybe_sequence ();

	void repeat (ContentList, int);

	/** Emitted when content has been added to or removed from the playlist; implies OrderChanged */
	mutable boost::signals2::signal<void ()> Changed;
	mutable boost::signals2::signal<void ()> OrderChanged;
	/** Emitted when something about a piece of our content has changed;
	 *  these emissions include when the position of the content changes.
	 *  Third parameter is true if signals are currently being emitted frequently.
	 */
	mutable boost::signals2::signal<void (boost::weak_ptr<Content>, int, bool)> ContentChanged;

private:
	void content_changed (boost::weak_ptr<Content>, int, bool);
	void reconnect ();

	/** List of content.  Kept sorted in position order. */
	ContentList _content;
	bool _sequence;
	bool _sequencing;
	std::list<boost::signals2::connection> _content_connections;
};

#endif
