/*
    Copyright (C) 2012-2021 Carl Hetherington <cth@carlh.net>

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


/** @file  test/client_server_test.cc
 *  @brief Test the remote encoding code.
 *  @ingroup feature
 */


#include "lib/content_factory.h"
#include "lib/cross.h"
#include "lib/dcp_video.h"
#include "lib/dcpomatic_log.h"
#include "lib/encode_server.h"
#include "lib/encode_server_description.h"
#include "lib/encode_server_finder.h"
#include "lib/file_log.h"
#include "lib/film.h"
#include "lib/image.h"
#include "lib/j2k_image_proxy.h"
#include "lib/player_video.h"
#include "lib/raw_image_proxy.h"
#include "test.h"
#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>


using std::list;
using std::make_shared;
using std::shared_ptr;
using std::weak_ptr;
using boost::thread;
using boost::optional;
using dcp::ArrayData;
using namespace dcpomatic;


void
do_remote_encode (shared_ptr<DCPVideo> frame, EncodeServerDescription description, ArrayData locally_encoded)
{
	ArrayData remotely_encoded;
	BOOST_REQUIRE_NO_THROW (remotely_encoded = frame->encode_remotely (description, 1200));

	BOOST_REQUIRE_EQUAL (locally_encoded.size(), remotely_encoded.size());
	BOOST_CHECK_EQUAL (memcmp (locally_encoded.data(), remotely_encoded.data(), locally_encoded.size()), 0);
}


BOOST_AUTO_TEST_CASE (client_server_test_rgb)
{
	auto image = make_shared<Image>(AV_PIX_FMT_RGB24, dcp::Size (1998, 1080), Image::Alignment::PADDED);
	uint8_t* p = image->data()[0];

	for (int y = 0; y < 1080; ++y) {
		uint8_t* q = p;
		for (int x = 0; x < 1998; ++x) {
			*q++ = x % 256;
			*q++ = y % 256;
			*q++ = (x + y) % 256;
		}
		p += image->stride()[0];
	}

	auto sub_image = make_shared<Image>(AV_PIX_FMT_BGRA, dcp::Size (100, 200), Image::Alignment::PADDED);
	p = sub_image->data()[0];
	for (int y = 0; y < 200; ++y) {
		uint8_t* q = p;
		for (int x = 0; x < 100; ++x) {
			*q++ = y % 256;
			*q++ = x % 256;
			*q++ = (x + y) % 256;
			*q++ = 1;
		}
		p += sub_image->stride()[0];
	}

	LogSwitcher ls (make_shared<FileLog>("build/test/client_server_test_rgb.log"));

	auto pvf = std::make_shared<PlayerVideo>(
		make_shared<RawImageProxy>(image),
		Crop (),
		optional<double> (),
		dcp::Size (1998, 1080),
		dcp::Size (1998, 1080),
		Eyes::BOTH,
		Part::WHOLE,
		ColourConversion(),
		VideoRange::FULL,
		weak_ptr<Content>(),
		optional<ContentTime>(),
		false
		);

	pvf->set_text (PositionImage(sub_image, Position<int>(50, 60)));

	auto frame = make_shared<DCPVideo> (
		pvf,
		0,
		24,
		200000000,
		Resolution::TWO_K
		);

	auto locally_encoded = frame->encode_locally ();

	auto server = make_shared<EncodeServer>(true, 2);

	thread server_thread(boost::bind(&EncodeServer::run, server));

	/* Let the server get itself ready */
	dcpomatic_sleep_seconds (1);

	/* "localhost" rather than "127.0.0.1" here fails on docker; go figure */
	EncodeServerDescription description ("127.0.0.1", 1, SERVER_LINK_VERSION);

	list<thread> threads;
	for (int i = 0; i < 8; ++i) {
		threads.push_back(thread(boost::bind(do_remote_encode, frame, description, locally_encoded)));
	}

	for (auto& i: threads) {
		i.join();
	}

	threads.clear();

	server->stop ();
	server_thread.join();
}


BOOST_AUTO_TEST_CASE (client_server_test_yuv)
{
	auto image = make_shared<Image>(AV_PIX_FMT_YUV420P, dcp::Size (1998, 1080), Image::Alignment::PADDED);

	for (int i = 0; i < image->planes(); ++i) {
		uint8_t* p = image->data()[i];
		for (int j = 0; j < image->line_size()[i]; ++j) {
			*p++ = j % 256;
		}
	}

	auto sub_image = make_shared<Image>(AV_PIX_FMT_BGRA, dcp::Size (100, 200), Image::Alignment::PADDED);
	uint8_t* p = sub_image->data()[0];
	for (int y = 0; y < 200; ++y) {
		uint8_t* q = p;
		for (int x = 0; x < 100; ++x) {
			*q++ = y % 256;
			*q++ = x % 256;
			*q++ = (x + y) % 256;
			*q++ = 1;
		}
		p += sub_image->stride()[0];
	}

	LogSwitcher ls (make_shared<FileLog>("build/test/client_server_test_yuv.log"));

	auto pvf = std::make_shared<PlayerVideo>(
		std::make_shared<RawImageProxy>(image),
		Crop(),
		optional<double>(),
		dcp::Size(1998, 1080),
		dcp::Size(1998, 1080),
		Eyes::BOTH,
		Part::WHOLE,
		ColourConversion(),
		VideoRange::FULL,
		weak_ptr<Content>(),
		optional<ContentTime>(),
		false
		);

	pvf->set_text (PositionImage(sub_image, Position<int>(50, 60)));

	auto frame = make_shared<DCPVideo>(
		pvf,
		0,
		24,
		200000000,
		Resolution::TWO_K
		);

	auto locally_encoded = frame->encode_locally ();

	auto server = make_shared<EncodeServer>(true, 2);

	thread server_thread(boost::bind(&EncodeServer::run, server));

	/* Let the server get itself ready */
	dcpomatic_sleep_seconds (1);

	/* "localhost" rather than "127.0.0.1" here fails on docker; go figure */
	EncodeServerDescription description ("127.0.0.1", 2, SERVER_LINK_VERSION);

	list<thread> threads;
	for (int i = 0; i < 8; ++i) {
		threads.push_back(thread(boost::bind(do_remote_encode, frame, description, locally_encoded)));
	}

	for (auto& i: threads) {
		i.join();
	}

	threads.clear();

	server->stop ();
	server_thread.join();
}


BOOST_AUTO_TEST_CASE (client_server_test_j2k)
{
	auto image = make_shared<Image>(AV_PIX_FMT_YUV420P, dcp::Size (1998, 1080), Image::Alignment::PADDED);

	for (int i = 0; i < image->planes(); ++i) {
		uint8_t* p = image->data()[i];
		for (int j = 0; j < image->line_size()[i]; ++j) {
			*p++ = j % 256;
		}
	}

	LogSwitcher ls (make_shared<FileLog>("build/test/client_server_test_j2k.log"));

	auto raw_pvf = std::make_shared<PlayerVideo> (
		std::make_shared<RawImageProxy>(image),
		Crop(),
		optional<double>(),
		dcp::Size(1998, 1080),
		dcp::Size(1998, 1080),
		Eyes::BOTH,
		Part::WHOLE,
		ColourConversion(),
		VideoRange::FULL,
		weak_ptr<Content>(),
		optional<ContentTime>(),
		false
		);

	auto raw_frame = make_shared<DCPVideo> (
		raw_pvf,
		0,
		24,
		200000000,
		Resolution::TWO_K
		);

	auto raw_locally_encoded = raw_frame->encode_locally ();

	auto j2k_pvf = std::make_shared<PlayerVideo> (
		std::make_shared<J2KImageProxy>(raw_locally_encoded, dcp::Size(1998, 1080), AV_PIX_FMT_XYZ12LE),
		Crop(),
		optional<double>(),
		dcp::Size(1998, 1080),
		dcp::Size(1998, 1080),
		Eyes::BOTH,
		Part::WHOLE,
		PresetColourConversion::all().front().conversion,
		VideoRange::FULL,
		weak_ptr<Content>(),
		optional<ContentTime>(),
		false
		);

	auto j2k_frame = make_shared<DCPVideo> (
		j2k_pvf,
		0,
		24,
		200000000,
		Resolution::TWO_K
		);

	auto j2k_locally_encoded = j2k_frame->encode_locally ();

	auto server = make_shared<EncodeServer>(true, 2);

	thread server_thread(boost::bind (&EncodeServer::run, server));

	/* Let the server get itself ready */
	dcpomatic_sleep_seconds (1);

	/* "localhost" rather than "127.0.0.1" here fails on docker; go figure */
	EncodeServerDescription description ("127.0.0.1", 2, SERVER_LINK_VERSION);

	list<thread> threads;
	for (int i = 0; i < 8; ++i) {
		threads.push_back(thread(boost::bind(do_remote_encode, j2k_frame, description, j2k_locally_encoded)));
	}

	for (auto& i: threads) {
		i.join();
	}

	threads.clear();

	server->stop ();
	server_thread.join();

	EncodeServerFinder::drop();
}


BOOST_AUTO_TEST_CASE(real_encode_with_server)
{
	Cleanup cl;

	auto content = content_factory(TestPaths::private_data() / "dolby_aurora.vob");
	auto film = new_test_film("real_encode_with_server", content, &cl);
	film->set_interop(false);

	EncodeServerFinder::instance();

	EncodeServer server(true, 4);
	thread server_thread(boost::bind(&EncodeServer::run, &server));

	make_and_verify_dcp(film);

	server.stop();
	server_thread.join();

	BOOST_CHECK(server.frames_encoded() > 0);
	EncodeServerFinder::drop();

	cl.run();
}

