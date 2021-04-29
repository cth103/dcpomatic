/*
    Copyright (C) 2017-2018 Carl Hetherington <cth@carlh.net>

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

/** @file  src/lib/active_captions.h
 *  @brief ActiveText class.
 */

#include "dcpomatic_time.h"
#include "player_text.h"
#include <boost/thread/mutex.hpp>
#include <list>
#include <map>

class TextContent;

/** @class ActiveText
 *  @brief A class to maintain information on active subtitles for Player.
 */
class ActiveText
{
public:
	ActiveText () {}

	ActiveText (ActiveText const&) = delete;
	ActiveText& operator= (ActiveText const&) = delete;

	std::list<PlayerText> get_burnt (dcpomatic::DCPTimePeriod period, bool always_burn_captions) const;
	void clear_before (dcpomatic::DCPTime time);
	void clear ();
	void add_from (std::weak_ptr<const TextContent> content, PlayerText ps, dcpomatic::DCPTime from);
	std::pair<PlayerText, dcpomatic::DCPTime> add_to (std::weak_ptr<const TextContent> content, dcpomatic::DCPTime to);
	bool have (std::weak_ptr<const TextContent> content) const;

private:
	class Period
	{
	public:
		Period () {}

		Period (PlayerText s, dcpomatic::DCPTime f)
			: subs (s)
			, from (f)
		{}

		PlayerText subs;
		dcpomatic::DCPTime from;
		boost::optional<dcpomatic::DCPTime> to;
	};

	typedef std::map<std::weak_ptr<const TextContent>, std::list<Period>, std::owner_less<std::weak_ptr<const TextContent>>> Map;

	mutable boost::mutex _mutex;
	Map _data;
};
