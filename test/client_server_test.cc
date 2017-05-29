/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

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
 *  @brief Test the server class.
 *  @ingroup specific
 *
 *  Create a test image and then encode it using the standard mechanism
 *  and also using a EncodeServer object running on localhost.  Compare the resulting
 *  encoded data to check that they are the same.
 */

#include "lib/encode_server.h"
#include "lib/image.h"
#include "lib/cross.h"
#include "lib/dcp_video.h"
#include "lib/player_video.h"
#include "lib/raw_image_proxy.h"
#include "lib/j2k_image_proxy.h"
#include "lib/encode_server_description.h"
#include "lib/file_log.h"
#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>

using std::list;
using boost::shared_ptr;
using boost::thread;
using boost::optional;
using dcp::Data;

void
do_remote_encode (shared_ptr<DCPVideo> frame, EncodeServerDescription description, Data locally_encoded)
{
	Data remotely_encoded;
	BOOST_CHECK_NO_THROW (remotely_encoded = frame->encode_remotely (description, 60));

	BOOST_CHECK_EQUAL (locally_encoded.size(), remotely_encoded.size());
	BOOST_CHECK_EQUAL (memcmp (locally_encoded.data().get(), remotely_encoded.data().get(), locally_encoded.size()), 0);
}

BOOST_AUTO_TEST_CASE (client_server_test_rgb)
{
	shared_ptr<Image> image (new Image (AV_PIX_FMT_RGB24, dcp::Size (1998, 1080), true));
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

	shared_ptr<Image> sub_image (new Image (AV_PIX_FMT_RGBA, dcp::Size (100, 200), true));
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

	shared_ptr<FileLog> log (new FileLog ("build/test/client_server_test_rgb.log"));

	shared_ptr<PlayerVideo> pvf (
		new PlayerVideo (
			shared_ptr<ImageProxy> (new RawImageProxy (image)),
			Crop (),
			optional<double> (),
			dcp::Size (1998, 1080),
			dcp::Size (1998, 1080),
			EYES_BOTH,
			PART_WHOLE,
			ColourConversion ()
			)
		);

	pvf->set_subtitle (PositionImage (sub_image, Position<int> (50, 60)));

	shared_ptr<DCPVideo> frame (
		new DCPVideo (
			pvf,
			0,
			24,
			200000000,
			RESOLUTION_2K,
			log
			)
		);

	Data locally_encoded = frame->encode_locally (boost::bind (&Log::dcp_log, log.get(), _1, _2));

	EncodeServer* server = new EncodeServer (log, true, 2);

	thread* server_thread = new thread (boost::bind (&EncodeServer::run, server));

	/* Let the server get itself ready */
	dcpomatic_sleep (1);

	EncodeServerDescription description ("localhost", 2);

	list<thread*> threads;
	for (int i = 0; i < 8; ++i) {
		threads.push_back (new thread (boost::bind (do_remote_encode, frame, description, locally_encoded)));
	}

	for (list<thread*>::iterator i = threads.begin(); i != threads.end(); ++i) {
		(*i)->join ();
	}

	for (list<thread*>::iterator i = threads.begin(); i != threads.end(); ++i) {
		delete *i;
	}

	server->stop ();
	server_thread->join ();
	delete server_thread;
	delete server;
}

BOOST_AUTO_TEST_CASE (client_server_test_yuv)
{
	shared_ptr<Image> image (new Image (AV_PIX_FMT_YUV420P, dcp::Size (1998, 1080), true));

	for (int i = 0; i < image->planes(); ++i) {
		uint8_t* p = image->data()[i];
		for (int j = 0; j < image->line_size()[i]; ++j) {
			*p++ = j % 256;
		}
	}

	shared_ptr<Image> sub_image (new Image (AV_PIX_FMT_RGBA, dcp::Size (100, 200), true));
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

	shared_ptr<FileLog> log (new FileLog ("build/test/client_server_test_yuv.log"));

	shared_ptr<PlayerVideo> pvf (
		new PlayerVideo (
			shared_ptr<ImageProxy> (new RawImageProxy (image)),
			Crop (),
			optional<double> (),
			dcp::Size (1998, 1080),
			dcp::Size (1998, 1080),
			EYES_BOTH,
			PART_WHOLE,
			ColourConversion ()
			)
		);

	pvf->set_subtitle (PositionImage (sub_image, Position<int> (50, 60)));

	shared_ptr<DCPVideo> frame (
		new DCPVideo (
			pvf,
			0,
			24,
			200000000,
			RESOLUTION_2K,
			log
			)
		);

	Data locally_encoded = frame->encode_locally (boost::bind (&Log::dcp_log, log.get(), _1, _2));

	EncodeServer* server = new EncodeServer (log, true, 2);

	thread* server_thread = new thread (boost::bind (&EncodeServer::run, server));

	/* Let the server get itself ready */
	dcpomatic_sleep (1);

	EncodeServerDescription description ("localhost", 2);

	list<thread*> threads;
	for (int i = 0; i < 8; ++i) {
		threads.push_back (new thread (boost::bind (do_remote_encode, frame, description, locally_encoded)));
	}

	for (list<thread*>::iterator i = threads.begin(); i != threads.end(); ++i) {
		(*i)->join ();
	}

	for (list<thread*>::iterator i = threads.begin(); i != threads.end(); ++i) {
		delete *i;
	}

	server->stop ();
	server_thread->join ();
	delete server_thread;
	delete server;
}

BOOST_AUTO_TEST_CASE (client_server_test_j2k)
{
	shared_ptr<Image> image (new Image (AV_PIX_FMT_YUV420P, dcp::Size (1998, 1080), true));

	for (int i = 0; i < image->planes(); ++i) {
		uint8_t* p = image->data()[i];
		for (int j = 0; j < image->line_size()[i]; ++j) {
			*p++ = j % 256;
		}
	}

	shared_ptr<FileLog> log (new FileLog ("build/test/client_server_test_j2k.log"));

	shared_ptr<PlayerVideo> raw_pvf (
		new PlayerVideo (
			shared_ptr<ImageProxy> (new RawImageProxy (image)),
			Crop (),
			optional<double> (),
			dcp::Size (1998, 1080),
			dcp::Size (1998, 1080),
			EYES_BOTH,
			PART_WHOLE,
			ColourConversion ()
			)
		);

	shared_ptr<DCPVideo> raw_frame (
		new DCPVideo (
			raw_pvf,
			0,
			24,
			200000000,
			RESOLUTION_2K,
			log
			)
		);

	Data raw_locally_encoded = raw_frame->encode_locally (boost::bind (&Log::dcp_log, log.get(), _1, _2));

	shared_ptr<PlayerVideo> j2k_pvf (
		new PlayerVideo (
			shared_ptr<ImageProxy> (new J2KImageProxy (raw_locally_encoded, dcp::Size (1998, 1080), AV_PIX_FMT_XYZ12LE)),
			Crop (),
			optional<double> (),
			dcp::Size (1998, 1080),
			dcp::Size (1998, 1080),
			EYES_BOTH,
			PART_WHOLE,
			PresetColourConversion::all().front().conversion
			)
		);

	shared_ptr<DCPVideo> j2k_frame (
		new DCPVideo (
			j2k_pvf,
			0,
			24,
			200000000,
			RESOLUTION_2K,
			log
			)
		);

	Data j2k_locally_encoded = j2k_frame->encode_locally (boost::bind (&Log::dcp_log, log.get(), _1, _2));

	EncodeServer* server = new EncodeServer (log, true, 2);

	thread* server_thread = new thread (boost::bind (&EncodeServer::run, server));

	/* Let the server get itself ready */
	dcpomatic_sleep (1);

	EncodeServerDescription description ("localhost", 2);

	list<thread*> threads;
	for (int i = 0; i < 8; ++i) {
		threads.push_back (new thread (boost::bind (do_remote_encode, j2k_frame, description, j2k_locally_encoded)));
	}

	for (list<thread*>::iterator i = threads.begin(); i != threads.end(); ++i) {
		(*i)->join ();
	}

	for (list<thread*>::iterator i = threads.begin(); i != threads.end(); ++i) {
		delete *i;
	}

	server->stop ();
	server_thread->join ();
	delete server_thread;
	delete server;
}
