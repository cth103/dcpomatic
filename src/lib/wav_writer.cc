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


#include "dcpomatic_assert.h"
#include "exceptions.h"
#include "wav_writer.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>


using std::vector;


static
void
put32(uint8_t* p, uint32_t v)
{
	p[0] = v & 0xff;
	p[1] = (v >> 8) & 0xff;
	p[2] = (v >> 16) & 0xff;
	p[3] = (v >> 24) & 0xff;
}


static
void
put16(uint8_t* p, uint16_t v)
{
	p[0] = v & 0xff;
	p[1] = (v >> 8) & 0xff;
}


static
vector<uint8_t>
header(int sample_rate, uint32_t data_bytes)
{
	vector<uint8_t> h(44);
	memcpy(h.data(), "RIFF", 4);
	put32(h.data() + 4, 36 + data_bytes);
	memcpy(h.data() + 8, "WAVEfmt ", 8);
	put32(h.data() + 16, 16);
	put16(h.data() + 20, 1);
	put16(h.data() + 22, 1);
	put32(h.data() + 24, sample_rate);
	put32(h.data() + 28, sample_rate * 3);
	put16(h.data() + 32, 3);
	put16(h.data() + 34, 24);
	memcpy(h.data() + 36, "data", 4);
	put32(h.data() + 40, data_bytes);
	return h;
}


WAVWriter::WAVWriter(boost::filesystem::path path, int sample_rate)
	: _path(path)
	, _sample_rate(sample_rate)
	, _file(dcp::File(path, "wb"))
{
	if (!*_file) {
		throw FileError("Could not open WAV file for writing", path);
	}
	auto const h = header(_sample_rate, 0);
	_file->checked_write(h.data(), h.size());
}


WAVWriter::~WAVWriter()
{
	try {
		close();
	} catch (...) {}
}


void
WAVWriter::write(float const* samples, int frames)
{
	DCPOMATIC_ASSERT(_file);

	vector<uint8_t> out(frames * 3);
	for (int i = 0; i < frames; ++i) {
		/* The same conversion as libdcp's SoundAssetWriter, so that these files hold
		 * exactly the samples that a DCP made from the same film would.
		 */
		float constexpr clip = 1.0f - (1.0f / (1 << 23));
		float constexpr scale = (1 << 23);
		auto const clipped = std::max(-clip, std::min(clip, samples[i]));
		int32_t const v = static_cast<int32_t>(std::lround(clipped * scale));
		out[i * 3] = v & 0xff;
		out[i * 3 + 1] = (v >> 8) & 0xff;
		out[i * 3 + 2] = (v >> 16) & 0xff;
	}

	_file->checked_write(out.data(), out.size());
	_data_bytes += out.size();
}


void
WAVWriter::close()
{
	if (!_file) {
		return;
	}

	auto const h = header(_sample_rate, _data_bytes);
	_file->seek(0, SEEK_SET);
	_file->checked_write(h.data(), h.size());
	_file->close();
	_file = boost::none;
}
