/*
    Copyright (C) 2018-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_SHOW_PLAYLIST_ENTRY_H
#define DCPOMATIC_SHOW_PLAYLIST_ENTRY_H


#include <dcp/content_kind.h>
#include <nlohmann/json.hpp>


class Content;


/** @class ShowPlaylistEntry
 *
 *  @brief An entry on a show playlist (SPL).
 *
 *  Given a UUID from the database, a ShowPlaylistEntry can be obtained from the
 *  ShowPlaylistList.
 */
class ShowPlaylistEntry
{
public:
	ShowPlaylistEntry(std::shared_ptr<Content> content, boost::optional<float> crop_to_ratio);
	ShowPlaylistEntry(
		std::string uuid,
		std::string name,
		dcp::ContentKind kind,
		std::string approximate_length,
		bool encrypted,
		boost::optional<float> crop_to_ratio
	);

	nlohmann::json as_json() const;

	std::string uuid() const;
	std::string name() const;
	dcp::ContentKind kind() const;
	std::string approximate_length() const;
	bool encrypted() const;
	boost::optional<float> crop_to_ratio() const;

	void set_crop_to_ratio(boost::optional<float> ratio);

private:
	std::string _uuid;
	std::string _name;
	dcp::ContentKind _kind;
	std::string _approximate_length;
	bool _encrypted;
	boost::optional<float> _crop_to_ratio;
};


bool operator==(ShowPlaylistEntry const& a, ShowPlaylistEntry const& b);
bool operator!=(ShowPlaylistEntry const& a, ShowPlaylistEntry const& b);


#endif
