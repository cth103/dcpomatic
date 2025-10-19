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
class ShowPlaylistEntry;


/** @class ShowPlaylistContentStore
 *  @brief Class to maintain details of content that can be included in show playlists
 */
class ShowPlaylistContentStore
{
public:
	/** @param uuid UUID, which can either be a CPL UUID (for a CPL in a DCP), or
	 *  a digest, for other content.
	 *  @return Content, or nullptr.
	 */
	std::shared_ptr<Content> get(std::string const& uuid) const;
	std::shared_ptr<Content> get(ShowPlaylistEntry const& entry) const;

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
