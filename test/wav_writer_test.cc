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


/** @file  test/wav_writer_test.cc
 *  @brief Tests of WAVWriter.
 *  @ingroup feature
 */


#include "lib/wav_writer.h"
#include <dcp/filesystem.h>
#include <boost/test/unit_test.hpp>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>


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
uint32_t
le32(vector<uint8_t> const& d, size_t offset)
{
	return uint32_t(d[offset]) | (uint32_t(d[offset + 1]) << 8) | (uint32_t(d[offset + 2]) << 16) | (uint32_t(d[offset + 3]) << 24);
}


static
uint16_t
le16(vector<uint8_t> const& d, size_t offset)
{
	return uint16_t(d[offset]) | (uint16_t(d[offset + 1]) << 8);
}


static
int32_t
sample24(vector<uint8_t> const& d, size_t index)
{
	size_t const o = 44 + index * 3;
	int32_t v = int32_t(d[o]) | (int32_t(d[o + 1]) << 8) | (int32_t(d[o + 2]) << 16);
	if (v & 0x800000) {
		v -= 0x1000000;
	}
	return v;
}


static
string
tag(vector<uint8_t> const& d, size_t offset)
{
	return string(d.begin() + offset, d.begin() + offset + 4);
}


BOOST_AUTO_TEST_CASE(wav_writer_header_test)
{
	boost::filesystem::path const path = "build/test/wav_writer_header_test.wav";
	dcp::filesystem::remove(path);
	{
		WAVWriter writer(path, 48000);
		vector<float> samples(100, 0.25f);
		writer.write(samples.data(), 100);
		writer.close();
	}

	auto const d = read_all(path);
	BOOST_REQUIRE_EQUAL(d.size(), 44U + 300U);
	BOOST_CHECK_EQUAL(tag(d, 0), "RIFF");
	BOOST_CHECK_EQUAL(le32(d, 4), d.size() - 8);
	BOOST_CHECK_EQUAL(tag(d, 8), "WAVE");
	BOOST_CHECK_EQUAL(tag(d, 12), "fmt ");
	BOOST_CHECK_EQUAL(le32(d, 16), 16U);
	BOOST_CHECK_EQUAL(le16(d, 20), 1);
	BOOST_CHECK_EQUAL(le16(d, 22), 1);
	BOOST_CHECK_EQUAL(le32(d, 24), 48000U);
	BOOST_CHECK_EQUAL(le32(d, 28), 48000U * 3);
	BOOST_CHECK_EQUAL(le16(d, 32), 3);
	BOOST_CHECK_EQUAL(le16(d, 34), 24);
	BOOST_CHECK_EQUAL(tag(d, 36), "data");
	BOOST_CHECK_EQUAL(le32(d, 40), 300U);
}


BOOST_AUTO_TEST_CASE(wav_writer_samples_test)
{
	boost::filesystem::path const path = "build/test/wav_writer_samples_test.wav";
	dcp::filesystem::remove(path);
	{
		WAVWriter writer(path, 48000);
		float const samples[] = { 0.0f, 1.0f, -1.0f, 0.5f };
		writer.write(samples, 4);
		writer.close();
	}

	auto const d = read_all(path);
	BOOST_REQUIRE_EQUAL(d.size(), 44U + 12U);
	BOOST_CHECK_CLOSE(double(sample24(d, 0)), 0.0, 1.0);
	BOOST_CHECK_EQUAL(sample24(d, 0), 0);
	BOOST_CHECK_EQUAL(sample24(d, 1), 8388607);
	/* Same conversion as libdcp's SoundAssetWriter, so a DCDM WAV holds exactly the DCP's samples */
	BOOST_CHECK_EQUAL(sample24(d, 2), -8388607);
	BOOST_CHECK_EQUAL(sample24(d, 3), 4194304);
}


BOOST_AUTO_TEST_CASE(wav_writer_clips_test)
{
	boost::filesystem::path const path = "build/test/wav_writer_clips_test.wav";
	dcp::filesystem::remove(path);
	{
		WAVWriter writer(path, 48000);
		float const samples[] = { 2.0f, -2.0f, 1.0000001f };
		writer.write(samples, 3);
		writer.close();
	}

	auto const d = read_all(path);
	BOOST_REQUIRE_EQUAL(d.size(), 44U + 9U);
	BOOST_CHECK_EQUAL(sample24(d, 0), 8388607);
	BOOST_CHECK_EQUAL(sample24(d, 1), -8388607);
	BOOST_CHECK_EQUAL(sample24(d, 2), 8388607);
}


BOOST_AUTO_TEST_CASE(wav_writer_close_in_destructor_test)
{
	boost::filesystem::path const path = "build/test/wav_writer_destructor_test.wav";
	dcp::filesystem::remove(path);
	{
		WAVWriter writer(path, 96000);
		float const samples[] = { 0.1f, 0.2f };
		writer.write(samples, 2);
	}

	auto const d = read_all(path);
	BOOST_REQUIRE_EQUAL(d.size(), 44U + 6U);
	BOOST_CHECK_EQUAL(le32(d, 24), 96000U);
	BOOST_CHECK_EQUAL(le32(d, 40), 6U);
}
