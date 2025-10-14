/*
    Copyright (C) 2018 Carl Hetherington <cth@carlh.net>

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


#include <functional>
#include <memory>
#include <vector>


class Content;


/** @class ShowPlaylistContentStore
 *  @brief Class to maintain details of content that can be included in show playlists
 */
class ShowPlaylistContentStore
{
public:
	std::shared_ptr<Content> get_by_digest(std::string digest) const;
	std::shared_ptr<Content> get_by_cpl_id(std::string id) const;

	/** Examine content in the configured directory and update our list.
	 *  @param pulse Called every so often to indicate progress.  Return false to cancel the scan.
	 *  @return Errors (first of the pair is the summary, second is the detail).
	 */
	std::vector<std::pair<std::string, std::string>> update(std::function<bool()> pulse);

	static ShowPlaylistContentStore* instance();

	std::vector<std::shared_ptr<Content>> const& all() const {
		return _content;
	}

private:
	std::vector<std::shared_ptr<Content>> _content;

	static ShowPlaylistContentStore* _instance;
};
