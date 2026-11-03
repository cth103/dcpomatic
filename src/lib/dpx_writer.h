/*
    Copyright (C) 2026 Carl Hetherington <cth@carlh.net>

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


/** @file  src/lib/dpx_writer.h
 *  @brief Writer of 12-bit XYZ DPX images.
 */


#ifndef DCPOMATIC_DPX_WRITER_H
#define DCPOMATIC_DPX_WRITER_H


#include <boost/filesystem.hpp>
#include <memory>


class Image;


/** Write a 12-bit CIE XYZ image as a DPX file (SMPTE 268M v2.0, big-endian).
 *
 *  DPX has no XYZ colorimetric code, so the file is described as 3-component
 *  "RGB" (descriptor 50), user-defined transfer and colorimetric, and the
 *  image element description says "CIE XYZ 12-bit".  XYZ is by convention.
 *
 *  Each component is stored as a big-endian 16-bit word with the 12-bit value
 *  left-justified (packing method A), which is how AV_PIX_FMT_XYZ12LE holds
 *  it, so words are copied with a byte swap only.
 *
 *  @param path Output file.
 *  @param image Image, which must be AV_PIX_FMT_XYZ12LE (else PixelFormatError is thrown).
 *  @throw FileError if the file cannot be written.
 */
void write_dpx(boost::filesystem::path const& path, std::shared_ptr<const Image> image);


#endif
