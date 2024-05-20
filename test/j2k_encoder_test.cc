/*
    Copyright (C) 2024 Carl Hetherington <cth@carlh.net>

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


#include "lib/config.h"
#include "lib/cross.h"
#include "lib/image.h"
#include "lib/j2k_encoder.h"
#include "lib/player_video.h"
#include "lib/raw_image_proxy.h"
#include "lib/writer.h"
#include "test.h"
extern "C" {
#include <libavutil/pixfmt.h>
}
#include <boost/test/unit_test.hpp>


using std::make_shared;
using std::weak_ptr;
using boost::optional;


BOOST_AUTO_TEST_CASE(j2k_encoder_deadlock_test)
{
	ConfigRestorer cr;

	auto film = new_test_film("j2k_encoder_deadlock_test");

	/* Don't call ::start() on this Writer, so it can never write anything */
	Writer writer(film, {}, {});
	writer.set_encoder_threads(4);

	/* We want to test the case where the writer queue fills, and this can't happen unless there
	 * are enough encoding threads (each of which will end up waiting for the writer to empty,
	 * which will never happen).
	 */
	Config::instance()->set_master_encoding_threads(4);
	J2KEncoder encoder(film, writer);
	encoder.begin();

	for (int i = 0; i < 26; ++i) {
		auto image = make_shared<Image>(AV_PIX_FMT_RGB24, dcp::Size(1998, 1080), Image::Alignment::PADDED);
		auto image_proxy = make_shared<RawImageProxy>(image);
		encoder.encode(
			std::make_shared<PlayerVideo>(
				image_proxy,
				Crop(),
				optional<double>(),
				dcp::Size(1998, 1080),
				dcp::Size(1998, 1080),
				Eyes::BOTH,
				Part::WHOLE,
				optional<ColourConversion>(),
				VideoRange::VIDEO,
				weak_ptr<Content>(),
				optional<dcpomatic::ContentTime>(),
				false
				),
			{}
			);
	}

	dcpomatic_sleep_seconds(10);
}

