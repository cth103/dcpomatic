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


/** @file  src/lib/wav_writer.h
 *  @brief Writer of mono 24-bit WAV files.
 */


#ifndef DCPOMATIC_WAV_WRITER_H
#define DCPOMATIC_WAV_WRITER_H


#include <dcp/file.h>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <cstdint>


/** @class WAVWriter
 *  @brief Writes a mono, 24-bit, little-endian PCM WAV file.
 *
 *  Float samples are converted to 24-bit integers exactly as libdcp's
 *  SoundAssetWriter does (clip to +/-(1 - 2^-23), scale by 2^23, round).  The RIFF and data sizes are patched in close().
 */
class WAVWriter
{
public:
	/** @param path Output file.
	 *  @param sample_rate Sample rate in Hz.
	 *  @throw FileError if the file cannot be opened.
	 */
	WAVWriter(boost::filesystem::path path, int sample_rate);
	~WAVWriter();

	WAVWriter(WAVWriter const&) = delete;
	WAVWriter& operator=(WAVWriter const&) = delete;

	/** Write some samples */
	void write(float const* samples, int frames);

	/** Patch the header and close the file.  Safe to call twice. */
	void close();

private:
	boost::filesystem::path _path;
	int _sample_rate;
	boost::optional<dcp::File> _file;
	uint32_t _data_bytes = 0;
};


#endif
