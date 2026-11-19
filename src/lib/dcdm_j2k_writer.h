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


/** @file  src/lib/dcdm_j2k_writer.h
 *  @brief Writer of lossless JPEG 2000 images for a DCDM.
 */


#ifndef DCPOMATIC_DCDM_J2K_WRITER_H
#define DCPOMATIC_DCDM_J2K_WRITER_H


#include <boost/filesystem.hpp>
#include <memory>


namespace dcp {
	class OpenJPEGImage;
}


/** Write a 12-bit X'Y'Z' image as a mathematically lossless JPEG 2000 codestream,
 *  following SMPTE ST 428-24 (DCDM packed image) as far as OpenJPEG allows:
 *  reversible 5/3 wavelet, 5 decomposition levels, RPCL progression, code-blocks of
 *  128x32, precincts of 256x256 (128x128 at the lowest resolution), tile-parts by
 *  resolution, TLM marker, and the COM marker
 *  "SMPTE DCDM X'Y'Z' image as defined in SMPTE ST 428-1".
 *
 *  The High-Throughput block coder that ST 428-24 recommends (Pcap15 = 1) is not used,
 *  because OpenJPEG cannot encode it; the part 1 block coder is still lossless.
 *
 *  @throw EncodeError if encoding fails.
 */
void write_lossless_j2k(boost::filesystem::path const& path, std::shared_ptr<const dcp::OpenJPEGImage> xyz);


#endif
