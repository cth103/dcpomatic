/*
    Copyright (C) 2020 Carl Hetherington <cth@carlh.net>

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

/* We have the front-end application dcpomatic2_disk and the back-end
 * dcpomatic2_disk_writer.  The communication is line-based, separated
 * by \n.
 */

/* REQUEST TO WRITE DCP */

// Front-end sends:

#define DISK_WRITER_WRITE "W"
// DCP pathname
// Internal name of the drive to write to

// Back-end responds:

// everything is ok
#define DISK_WRITER_OK "D"

// there was an error
#define DISK_WRITER_ERROR "E"
// Error message
// Error number

// the drive is being formatted
#define DISK_WRITER_FORMATTING "F"

// data is being copied, 30% done
#define DISK_WRITER_COPY_PROGRESS "C"
// 0.3\n

// data is being verified, 60% done
#define DISK_WRITER_VERIFY_PROGRESS "V"
// 0.6\n


/* REQUEST TO QUIT */

// Front-end sends:
#define DISK_WRITER_QUIT "Q"


/* REQUEST TO UNMOUNT A DRIVE */

// Front-end sends:
#define DISK_WRITER_UNMOUNT "U"
// XML representation of Drive object to unmount

// Back-end responds:
// DISK_WRITER_OK
// or
// DISK_WRITER_ERROR

