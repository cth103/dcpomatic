/*
    Copyright (C) 2019-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_OPTIMISATION_H
#define DCPOMATIC_OPTIMISATION_H


/** An optimisation that can be requested of the FilmViewer if it is known that
 *  all content will be the same type.
 */
enum class Optimisation
{
	JPEG2000, ///< all content is JPEG2000
	MPEG2,    ///< all content is Interop MPEG2
	NONE,     ///< viewer must be prepared for anything
};


#endif
