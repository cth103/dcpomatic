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


#ifndef DCPOMATIC_CROP_H
#define DCPOMATIC_CROP_H


#include <dcp/types.h>
#include <memory>


namespace cxml {
	class Node;
}


/** @struct Crop
 *  @brief A description of the crop of an image or video.
 */
struct Crop
{
	Crop() {}
	Crop(int l, int r, int t, int b) : left (l), right (r), top (t), bottom (b) {}
	explicit Crop(std::shared_ptr<cxml::Node>);

	/** Number of pixels to remove from the left-hand side */
	int left = 0;
	/** Number of pixels to remove from the right-hand side */
	int right = 0;
	/** Number of pixels to remove from the top */
	int top = 0;
	/** Number of pixels to remove from the bottom */
	int bottom = 0;

	dcp::Size apply(dcp::Size s, int minimum = 4) const {
		s.width -= left + right;
		s.height -= top + bottom;

		if (s.width < minimum) {
			s.width = minimum;
		}

		if (s.height < minimum) {
			s.height = minimum;
		}

		return s;
	}

	void as_xml (xmlpp::Node *) const;
};


extern bool operator==(Crop const & a, Crop const & b);
extern bool operator!=(Crop const & a, Crop const & b);


#endif
