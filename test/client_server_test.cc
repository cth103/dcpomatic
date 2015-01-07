/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

/** @file  test/client_server_test.cc
 *  @brief Test the server class.
 *
 *  Create a test image and then encode it using the standard mechanism
 *  and also using a Server object running on localhost.  Compare the resulting
 *  encoded data to check that they are the same.
 */

#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>
#include "lib/server.h"
#include "lib/image.h"
#include "lib/cross.h"
#include "lib/dcp_video.h"
#include "lib/scaler.h"
#include "lib/player_video.h"
#include "lib/raw_image_proxy.h"
#include "lib/encoded_data.h"

using std::list;
using boost::shared_ptr;
using boost::thread;
using boost::optional;

void
do_remote_encode (shared_ptr<DCPVideo> frame, ServerDescription description, shared_ptr<EncodedData> locally_encoded)
{
	shared_ptr<EncodedData> remotely_encoded;
	BOOST_CHECK_NO_THROW (remotely_encoded = frame->encode_remotely (description));
	BOOST_CHECK (remotely_encoded);
	
	BOOST_CHECK_EQUAL (locally_encoded->size(), remotely_encoded->size());
	BOOST_CHECK_EQUAL (memcmp (locally_encoded->data(), remotely_encoded->data(), locally_encoded->size()), 0);
}

BOOST_AUTO_TEST_CASE (client_server_test_rgb)
{
	shared_ptr<Image> image (new Image (PIX_FMT_RGB24, dcp::Size (1998, 1080), true));
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

	shared_ptr<Image> sub_image (new Image (PIX_FMT_RGBA, dcp::Size (100, 200), true));
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
			DCPTime (),
			Crop (),
			optional<float> (),
			dcp::Size (1998, 1080),
			dcp::Size (1998, 1080),
			Scaler::from_id ("bicubic"),
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
			true,
			log
			)
		);

	shared_ptr<EncodedData> locally_encoded = frame->encode_locally (boost::bind (&Log::dcp_log, log.get(), _1, _2));
	BOOST_ASSERT (locally_encoded);
	
	Server* server = new Server (log, true);

	new thread (boost::bind (&Server::run, server, 2));

	/* Let the server get itself ready */
	dcpomatic_sleep (1);

	ServerDescription description ("localhost", 2);

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

	delete server;
}

BOOST_AUTO_TEST_CASE (client_server_test_yuv)
{
	shared_ptr<Image> image (new Image (PIX_FMT_YUV420P, dcp::Size (1998, 1080), true));
	uint8_t* p = image->data()[0];

	for (int i = 0; i < image->components(); ++i) {
		uint8_t* p = image->data()[i];
		for (int j = 0; j < image->line_size()[i]; ++j) {
			*p++ = j % 256;
		}
	}

	shared_ptr<Image> sub_image (new Image (PIX_FMT_RGBA, dcp::Size (100, 200), true));
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

	shared_ptr<FileLog> log (new FileLog ("build/test/client_server_test_yuv.log"));

	shared_ptr<PlayerVideo> pvf (
		new PlayerVideo (
			shared_ptr<ImageProxy> (new RawImageProxy (image)),
			DCPTime (),
			Crop (),
			optional<float> (),
			dcp::Size (1998, 1080),
			dcp::Size (1998, 1080),
			Scaler::from_id ("bicubic"),
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
			true,
			log
			)
		);

	shared_ptr<EncodedData> locally_encoded = frame->encode_locally (boost::bind (&Log::dcp_log, log.get(), _1, _2));
	BOOST_ASSERT (locally_encoded);
	
	Server* server = new Server (log, true);

	new thread (boost::bind (&Server::run, server, 2));

	/* Let the server get itself ready */
	dcpomatic_sleep (1);

	ServerDescription description ("localhost", 2);

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

	delete server;
}

