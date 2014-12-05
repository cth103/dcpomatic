/*
    Copyright (C) 2013 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_PLAYLIST_H
#define DCPOMATIC_PLAYLIST_H

#include "ffmpeg_content.h"
#include "audio_mapping.h"
#include "util.h"
#include "frame_rate_change.h"
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <list>

class Content;
class FFmpegContent;
class FFmpegDecoder;
class StillImageMagickContent;
class StillImageMagickDecoder;
class SndfileContent;
class SndfileDecoder;
class Job;
class Film;
class Region;

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
	
	int best_dcp_frame_rate () const;
	DCPTime video_end () const;
	FrameRateChange active_frame_rate_change (DCPTime, int dcp_frame_rate) const;

	void set_sequence_video (bool);
	void maybe_sequence_video ();

	void repeat (ContentList, int);

	/** Emitted when content has been added to or removed from the playlist */
	mutable boost::signals2::signal<void ()> Changed;
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
	bool _sequence_video;
	bool _sequencing_video;
	std::list<boost::signals2::connection> _content_connections;
};

#endif
