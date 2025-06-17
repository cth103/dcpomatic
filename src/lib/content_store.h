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


#include <memory>


class Content;


/** @class ContentStore
 *  @brief Parent for classes which store content and can return content with a given digest or CPL ID.
 */
class ContentStore
{
public:
	virtual std::shared_ptr<Content> get_by_digest(std::string digest) const = 0;
	virtual std::shared_ptr<Content> get_by_cpl_id(std::string id) const = 0;
};
