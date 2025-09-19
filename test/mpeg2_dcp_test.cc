/*
    Copyright (C) 2025 Carl Hetherington <cth@carlh.net>

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


#include "test.h"
#include "lib/content_factory.h"
#include "lib/dcp_content.h"
#include "lib/film.h"
#include "lib/video_encoding.h"
#ifdef DCPOMATIC_LINUX
#include <boost/process.hpp>
#endif
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::shared_ptr;
using std::string;
using std::vector;


static
float
mbits_per_second(shared_ptr<const Film> film)
{
	auto const mxf = find_file(film->dir(film->dcp_name()), "mpeg2");
	auto const size = boost::filesystem::file_size(mxf);
	auto const seconds = film->length().seconds();
	return ((size * 8) / seconds) / 1e6;
}


#ifdef DCPOMATIC_LINUX

#ifdef DCPOMATIC_BOOST_PROCESS_V1
static
string
bitrate_in_header(shared_ptr<const Film> film)
{
	namespace bp = boost::process;

	bp::ipstream stream;
	bp::child child(boost::process::search_path("mediainfo"), find_file(film->dir(film->dcp_name()), "mpeg2"), bp::std_out > stream);

	string line;
	string rate;
	while (child.running() && std::getline(stream, line)) {
		if (line.substr(0, 10) == "Bit rate  ") {
			auto colon = line.find(":");
			if (colon != string::npos) {
				rate = line.substr(colon + 2);
			}
		}
	}

	child.wait();

	return rate;
}

#else

static
string
bitrate_in_header(shared_ptr<const Film> film)
{
	namespace bp = boost::process::v2;

	boost::asio::io_context context;
	boost::asio::readable_pipe out{context};
	bp::process child(context, bp::environment::find_executable("mediainfo"), { find_file(film->dir(film->dcp_name()), "mpeg2").string() }, bp::process_stdio{{}, out, {}});

	string output;
	boost::system::error_code ec;
	while (child.running()) {
		string block;
		boost::asio::read(out, boost::asio::dynamic_buffer(block), ec);
		output += block;
		if (ec && ec == boost::asio::error::eof) {
			break;
		}
	}

	vector<string> lines;
	boost::algorithm::split(lines, output, boost::is_any_of("\n"));

	string rate;
	for (auto const& line: lines) {
		if (line.substr(0, 10) == "Bit rate  ") {
			auto colon = line.find(":");
			if (colon != string::npos) {
				rate = line.substr(colon + 2);
			}
		}
	}

	child.wait();

	return rate;
}

#endif
#endif


BOOST_AUTO_TEST_CASE(mpeg2_video_bitrate1)
{
	auto content = content_factory(TestPaths::private_data() / "boon_telly.mkv");
	auto film = new_test_film("mpeg2_video_bitrate", content);
	film->set_video_bit_rate(VideoEncoding::MPEG2, 25000000);
	film->set_video_encoding(VideoEncoding::MPEG2);
	film->set_interop(true);

	make_and_verify_dcp(
		film,
		{ dcp::VerificationNote::Code::INVALID_STANDARD },
		false, false
	);

	BOOST_CHECK_CLOSE(mbits_per_second(film), 25.0047, 0.001);
#ifdef DCPOMATIC_LINUX
	BOOST_CHECK_EQUAL(bitrate_in_header(film), "25.0 Mb/s");
#endif
}


BOOST_AUTO_TEST_CASE(mpeg2_video_bitrate2)
{
	auto content = make_shared<DCPContent>(TestPaths::private_data() / "JourneyToJah_TLR-1_F_EN-DE-FR_CH_51_2K_LOK_20140225_DGL_SMPTE_OV");
	auto film = new_test_film("mpeg2_video_bitrate", { content });
	film->set_video_bit_rate(VideoEncoding::MPEG2, 5000000);
	film->set_video_encoding(VideoEncoding::MPEG2);
	film->set_interop(true);

	make_and_verify_dcp(
		film,
		{ dcp::VerificationNote::Code::INVALID_STANDARD },
		false, false
	);

	BOOST_CHECK_CLOSE(mbits_per_second(film), 5.01890659, 0.05);
#ifdef DCPOMATIC_LINUX
	BOOST_CHECK_EQUAL(bitrate_in_header(film), "5 000 kb/s");
#endif
}
