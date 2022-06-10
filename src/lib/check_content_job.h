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

#include "job.h"

/** @class CheckContentJob
 *  @brief A job to check content that was just loaded to see if anything needs fixing about it.
 */

class CheckContentJob : public Job
{
public:
	CheckContentJob (std::shared_ptr<const Film>);
	~CheckContentJob ();

	std::string name () const override;
	std::string json_name () const override;
	void run () override;
};
