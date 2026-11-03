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


#include "dpx_writer.h"
#include "exceptions.h"
#include "image.h"
#include <dcp/file.h>
#include <cstring>
#include <vector>


using std::shared_ptr;
using std::string;
using std::vector;


static int constexpr HEADERS_SIZE = 2048;
static int constexpr GENERIC_HEADER_SIZE = 1664;
static int constexpr INDUSTRY_HEADER_SIZE = 384;


static
void
put32(vector<uint8_t>& h, size_t offset, uint32_t value)
{
	h[offset] = (value >> 24) & 0xff;
	h[offset + 1] = (value >> 16) & 0xff;
	h[offset + 2] = (value >> 8) & 0xff;
	h[offset + 3] = value & 0xff;
}


static
void
put16(vector<uint8_t>& h, size_t offset, uint16_t value)
{
	h[offset] = (value >> 8) & 0xff;
	h[offset + 1] = value & 0xff;
}


static
void
put_string(vector<uint8_t>& h, size_t offset, size_t max, string s)
{
	memcpy(h.data() + offset, s.c_str(), std::min(max, s.size()));
}


void
write_dpx(boost::filesystem::path const& path, shared_ptr<const Image> image)
{
	if (image->pixel_format() != AV_PIX_FMT_XYZ12LE) {
		throw PixelFormatError("write_dpx", image->pixel_format());
	}

	auto const size = image->size();
	uint32_t const data_size = size.width * size.height * 3 * 2;

	/* Undefined integer fields are 0xffffffff in DPX; start from that for the
	 * whole header and then set what we know.  ASCII fields are zeroed below.
	 */
	vector<uint8_t> h(HEADERS_SIZE, 0xff);
	auto clear = [&h](size_t offset, size_t length) {
		memset(h.data() + offset, 0, length);
	};

	/* File information header (768 bytes) */
	put32(h, 0, 0x53445058);
	put32(h, 4, HEADERS_SIZE);
	clear(8, 8);
	put_string(h, 8, 8, "V2.0");
	put32(h, 16, HEADERS_SIZE + data_size);
	put32(h, 20, 1);
	put32(h, 24, GENERIC_HEADER_SIZE);
	put32(h, 28, INDUSTRY_HEADER_SIZE);
	put32(h, 32, 0);
	clear(36, 100 + 24 + 100 + 200 + 200);
	put_string(h, 36, 100, path.filename().string());
	put_string(h, 160, 100, "DCP-o-matic");
	clear(664, 104);

	/* Image information header (640 bytes) */
	put16(h, 768, 0);
	put16(h, 770, 1);
	put32(h, 772, size.width);
	put32(h, 776, size.height);

	/* Image element 0 (72 bytes) */
	size_t const e = 780;
	put32(h, e + 0, 0);
	put32(h, e + 4, 0);
	put32(h, e + 12, 4095);
	h[e + 20] = 50;
	h[e + 21] = 0;
	h[e + 22] = 0;
	h[e + 23] = 12;
	put16(h, e + 24, 1);
	put16(h, e + 26, 0);
	put32(h, e + 28, HEADERS_SIZE);
	put32(h, e + 32, 0);
	put32(h, e + 36, 0);
	clear(e + 40, 32);
	put_string(h, e + 40, 32, "CIE XYZ 12-bit");
	/* Remaining image elements are unused: sign 0, rest undefined */
	for (int i = 1; i < 8; ++i) {
		clear(e + i * 72, 4);
		clear(e + i * 72 + 20, 8);
		clear(e + i * 72 + 40, 32);
	}
	clear(780 + 576, 52);

	/* Image source information, orientation (256 bytes at 1408) and film / TV headers:
	 * leave undefined for integers, clear ASCII.
	 */
	clear(1408, 256);
	clear(1664, INDUSTRY_HEADER_SIZE);

	dcp::File file(path, "wb");
	if (!file) {
		throw FileError("Could not open DPX file for writing", path);
	}
	file.checked_write(h.data(), h.size());

	vector<uint8_t> row(size.width * 3 * 2);
	for (int y = 0; y < size.height; ++y) {
		auto const* in = reinterpret_cast<uint16_t const*>(image->data()[0] + y * image->stride()[0]);
		for (int i = 0; i < size.width * 3; ++i) {
			row[i * 2] = in[i] >> 8;
			row[i * 2 + 1] = in[i] & 0xff;
		}
		file.checked_write(row.data(), row.size());
	}
	file.close();
}
