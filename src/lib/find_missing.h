/*
    Copyright (C) 2021 Carl Hetherington <cth@carlh.net>

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


#include <boost/filesystem.hpp>
#include <memory>
#include <vector>


class Content;


namespace dcpomatic {


/** Try to resolve some missing content file paths using a clue.  On return
 *  any content whose files were found will have been updated.
 *
 *  @param content Content, some of which may have missing files.
 *  @param clue Path to a file which gives a clue about where the missing files might be.
 */
void find_missing (std::vector<std::shared_ptr<Content>> content, boost::filesystem::path clue);


}

