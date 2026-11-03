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


/** @file  test/dcdm_j2k_writer_test.cc
 *  @brief Tests of the lossless JPEG 2000 writer for DCDM images.
 *  @ingroup feature
 */


#include "lib/dcdm_j2k_writer.h"
#include <dcp/filesystem.h>
#include <dcp/j2k_transcode.h>
#include <dcp/openjpeg_image.h>
#include <boost/test/unit_test.hpp>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>


using std::make_shared;
using std::string;
using std::vector;


static
vector<uint8_t>
read_all(boost::filesystem::path path)
{
	std::ifstream in(path.string(), std::ios::binary);
	return vector<uint8_t>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}


static
std::shared_ptr<dcp::OpenJPEGImage>
make_xyz(int width, int height)
{
	auto image = make_shared<dcp::OpenJPEGImage>(dcp::Size(width, height));
	for (int c = 0; c < 3; ++c) {
		for (int i = 0; i < width * height; ++i) {
			/* Busy, full-range 12-bit content so that any loss would show */
			image->data(c)[i] = (i * 37 + c * 1013 + (i / width) * 7) % 4096;
		}
	}
	return image;
}


/** @return offset of the first marker @param marker in the main header, or -1 */
static
int
find_marker(vector<uint8_t> const& d, uint16_t marker)
{
	/* Main header markers follow SIZ; walk them until SOT */
	size_t i = 2;
	while (i + 4 <= d.size()) {
		uint16_t const m = (d[i] << 8) | d[i + 1];
		if (m == marker) {
			return static_cast<int>(i);
		}
		if (m == 0xff90) {
			return -1;
		}
		uint16_t const length = (d[i + 2] << 8) | d[i + 3];
		i += 2 + length;
	}
	return -1;
}


BOOST_AUTO_TEST_CASE(dcdm_j2k_lossless_test)
{
	auto const image = make_xyz(256, 120);
	boost::filesystem::path const path = "build/test/dcdm_j2k_lossless_test.pdc.j2c";
	dcp::filesystem::remove(path);
	write_lossless_j2k(path, image);

	auto const data = read_all(path);
	auto const decoded = dcp::decompress_j2k(data.data(), static_cast<int64_t>(data.size()), 0);
	BOOST_REQUIRE(decoded->size() == image->size());
	int64_t differences = 0;
	for (int c = 0; c < 3; ++c) {
		for (int i = 0; i < 256 * 120; ++i) {
			if (decoded->data(c)[i] != image->data(c)[i]) {
				++differences;
			}
		}
	}
	BOOST_CHECK_EQUAL(differences, 0);
}


BOOST_AUTO_TEST_CASE(dcdm_j2k_codestream_test)
{
	boost::filesystem::path const path = "build/test/dcdm_j2k_codestream_test.pdc.j2c";
	dcp::filesystem::remove(path);
	write_lossless_j2k(path, make_xyz(256, 120));
	auto const d = read_all(path);

	/* SOC, then SIZ with three 12-bit unsigned components at 1:1 sampling */
	BOOST_REQUIRE(d.size() > 100);
	BOOST_CHECK_EQUAL((d[0] << 8) | d[1], 0xff4f);
	auto const siz = find_marker(d, 0xff51);
	BOOST_REQUIRE(siz > 0);
	BOOST_CHECK_EQUAL((d[siz + 38] << 8) | d[siz + 39], 3);
	for (int c = 0; c < 3; ++c) {
		BOOST_CHECK_EQUAL(d[siz + 40 + c * 3], 11);
		BOOST_CHECK_EQUAL(d[siz + 41 + c * 3], 1);
		BOOST_CHECK_EQUAL(d[siz + 42 + c * 3], 1);
	}

	/* COD: RPCL progression (2), 5 decomposition levels, reversible 5/3 transform (1) */
	auto const cod = find_marker(d, 0xff52);
	BOOST_REQUIRE(cod > 0);
	BOOST_CHECK_EQUAL(d[cod + 5], 2);
	BOOST_CHECK_EQUAL(d[cod + 9], 5);
	BOOST_CHECK_EQUAL(d[cod + 13], 1);

	/* COM with the ST 428-24 text (Rcom = 1, Latin text) */
	auto const com = find_marker(d, 0xff64);
	BOOST_REQUIRE(com > 0);
	uint16_t const com_length = (d[com + 2] << 8) | d[com + 3];
	BOOST_CHECK_EQUAL((d[com + 4] << 8) | d[com + 5], 1);
	BOOST_CHECK_EQUAL(string(d.begin() + com + 6, d.begin() + com + 2 + com_length), "SMPTE DCDM X'Y'Z' image as defined in SMPTE ST 428-1");

	/* TLM is present, as in the standard's example encodings */
	BOOST_CHECK(find_marker(d, 0xff55) > 0);
}


/** Paths with non-ASCII characters are written through dcp::File (this matters on Windows) */
BOOST_AUTO_TEST_CASE(dcdm_j2k_unicode_path_test)
{
	boost::filesystem::path const dir = boost::filesystem::path("build/test") / boost::filesystem::path(u8"dcdm_j2k_unicode_éèç");
	dcp::filesystem::remove_all(dir);
	dcp::filesystem::create_directories(dir);
	auto const path = dir / "00000000.pdc.j2c";
	write_lossless_j2k(path, make_xyz(64, 32));
	BOOST_CHECK(dcp::filesystem::exists(path));
	auto const data = read_all(path);
	auto const decoded = dcp::decompress_j2k(data.data(), static_cast<int64_t>(data.size()), 0);
	BOOST_CHECK(decoded->size() == dcp::Size(64, 32));
	dcp::filesystem::remove_all(dir);
}
