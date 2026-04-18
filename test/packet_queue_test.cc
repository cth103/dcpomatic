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


#include "lib/subtitle_sync_packet_queue.h"
extern "C" {
#include <libavcodec/packet.h>
}
#include <boost/test/unit_test.hpp>


BOOST_AUTO_TEST_CASE(subtitle_sync_packet_queue_test)
{
	SubtitleSyncPacketQueue queue;
	std::vector<std::pair<AVPacket*, PacketQueue::Type>> expected;

	auto add_video = [&queue, &expected]() {
		auto packet = av_packet_alloc();
		queue.add(packet, PacketQueue::Type::VIDEO);
		expected.push_back(std::make_pair(packet, PacketQueue::Type::VIDEO));
		return packet;
	};

	auto add_audio = [&queue, &expected]() {
		auto packet = av_packet_alloc();
		queue.add(packet, PacketQueue::Type::AUDIO);
		expected.push_back(std::make_pair(packet, PacketQueue::Type::AUDIO));
		return packet;
	};

	auto add_subtitle = [&queue]() {
		auto packet = av_packet_alloc();
		queue.add(packet, PacketQueue::Type::SUBTITLE);
		return packet;
	};

	auto check = [&queue](bool flush, std::pair<AVPacket*, PacketQueue::Type> ref) {
		auto check = queue.get(flush);
		BOOST_CHECK(check.has_value());
		BOOST_CHECK(boost::get<AVPacket*>(check->first) == ref.first);
		BOOST_CHECK(check->second == ref.second);
	};

	for (int i = 0; i < 24; ++i) {
		add_video();
		add_audio();
		add_audio();
	}

	BOOST_CHECK(queue.get(false) == boost::none);

	for (int i = 0; i < 23; ++i) {
		add_video();
		add_audio();
		add_audio();
	}

	BOOST_CHECK(queue.get(false) == boost::none);

	auto iter = expected.begin();

	add_video();
	check(false, *iter);
	++iter;

	auto sub1 = add_subtitle();
	auto sub2 = add_subtitle();

	for (int i = 0; i < 24; ++i) {
		add_video();
		add_audio();
		add_audio();
	}

	check(false, { sub1, PacketQueue::Type::SUBTITLE });
	check(false, { sub2, PacketQueue::Type::SUBTITLE });

	for (int i = 0; i < 24; ++i) {
		check(false, *iter);
		++iter;
		check(false, *iter);
		++iter;
		check(false, *iter);
		++iter;
	}

	for (int i = 0; i < 47; ++i) {
		check(true, *iter);
		BOOST_REQUIRE(iter != expected.end());
		++iter;
		check(true, *iter);
		BOOST_REQUIRE(iter != expected.end());
		++iter;
		check(true, *iter);
		BOOST_REQUIRE(iter != expected.end());
		++iter;
	}
}

