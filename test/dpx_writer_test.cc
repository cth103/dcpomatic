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


/** @file  test/dpx_writer_test.cc
 *  @brief Tests of the DPX writer.
 *  @ingroup feature
 */


#include "lib/dpx_writer.h"
#include "lib/exceptions.h"
#include "lib/image.h"
#include <dcp/filesystem.h>
#include <boost/test/unit_test.hpp>
#include <fstream>
#include <iterator>
#include <vector>


using std::make_shared;
using std::vector;


static
vector<uint8_t>
read_all(boost::filesystem::path path)
{
	std::ifstream in(path.string(), std::ios::binary);
	return vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}


static
uint32_t
be32(vector<uint8_t> const& d, size_t offset)
{
	return (uint32_t(d[offset]) << 24) | (uint32_t(d[offset + 1]) << 16) | (uint32_t(d[offset + 2]) << 8) | uint32_t(d[offset + 3]);
}


static
uint16_t
be16(vector<uint8_t> const& d, size_t offset)
{
	return (uint16_t(d[offset]) << 8) | uint16_t(d[offset + 1]);
}


/** Make a width x height XYZ12LE image where component c of pixel (x, y) is value(x, y, c) << 4 */
template <class F>
static
std::shared_ptr<Image>
make_xyz_image(int width, int height, F value)
{
	auto image = make_shared<Image>(AV_PIX_FMT_XYZ12LE, dcp::Size(width, height), Image::Alignment::COMPACT);
	for (int y = 0; y < height; ++y) {
		auto row = reinterpret_cast<uint16_t*>(image->data()[0] + y * image->stride()[0]);
		for (int x = 0; x < width; ++x) {
			for (int c = 0; c < 3; ++c) {
				row[x * 3 + c] = static_cast<uint16_t>(value(x, y, c) << 4);
			}
		}
	}
	return image;
}


BOOST_AUTO_TEST_CASE(dpx_writer_header_test)
{
	auto image = make_xyz_image(4, 2, [](int, int, int) { return 100; });
	boost::filesystem::path const path = "build/test/dpx_writer_header_test.dpx";
	dcp::filesystem::remove(path);
	write_dpx(path, image);

	auto const d = read_all(path);
	BOOST_REQUIRE(d.size() > 2048);
	BOOST_CHECK_EQUAL(be32(d, 0), 0x53445058U);
	BOOST_CHECK_EQUAL(be32(d, 16), d.size());
	/* Image info header starts at 768: orientation(2) elements(2) pixels(4) lines(4) */
	BOOST_CHECK_EQUAL(be16(d, 770), 1);
	BOOST_CHECK_EQUAL(be32(d, 772), 4U);
	BOOST_CHECK_EQUAL(be32(d, 776), 2U);
	/* Element 0 starts at 780: descriptor at +20, bit depth at +23 */
	BOOST_CHECK_EQUAL(d[780 + 20], 50);
	BOOST_CHECK_EQUAL(d[780 + 23], 12);
	/* Data offset (file header offset 4) is after the 2048-byte headers */
	BOOST_CHECK_EQUAL(be32(d, 4), 2048U);
	BOOST_CHECK_EQUAL(d.size(), 2048U + 4 * 2 * 3 * 2);
}


BOOST_AUTO_TEST_CASE(dpx_writer_roundtrip_test)
{
	int const values[] = { 0, 4095, 2048, 1, 4094, 123, 3000, 17, 2999 };
	auto image = make_xyz_image(4, 2, [&](int x, int y, int c) { return values[(x + y * 4 + c) % 9]; });
	boost::filesystem::path const path = "build/test/dpx_writer_roundtrip_test.dpx";
	dcp::filesystem::remove(path);
	write_dpx(path, image);

	auto const d = read_all(path);
	auto const offset = be32(d, 4);
	for (int y = 0; y < 2; ++y) {
		for (int x = 0; x < 4; ++x) {
			for (int c = 0; c < 3; ++c) {
				/* 12-bit values are left-justified in big-endian 16-bit words (packing method A) */
				auto const word = be16(d, offset + ((y * 4 + x) * 3 + c) * 2);
				BOOST_CHECK_EQUAL(word >> 4, values[(x + y * 4 + c) % 9]);
				BOOST_CHECK_EQUAL(word & 0xf, 0);
			}
		}
	}
}


BOOST_AUTO_TEST_CASE(dpx_writer_rejects_wrong_format_test)
{
	auto image = make_shared<Image>(AV_PIX_FMT_RGB24, dcp::Size(4, 2), Image::Alignment::COMPACT);
	BOOST_CHECK_THROW(write_dpx("build/test/dpx_writer_wrong_format.dpx", image), PixelFormatError);
}
