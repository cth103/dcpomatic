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


#include "dcpomatic_log.h"
#include "subtitle_sync_packet_queue.h"
extern "C" {
#include <libavcodec/packet.h>
#include <libavutil/avutil.h>
}
#include <iostream>


using boost::optional;


void
SubtitleSyncPacketQueue::add(AVPacket* packet, Type type)
{
	switch (type) {
	case Type::VIDEO:
		++_num_video;
		_other.push_back({packet, type});
		break;
	case Type::AUDIO:
		_other.push_back({packet, type});
		break;
	case Type::DROP:
		_other.push_back({PacketInfo(packet), type});
		av_packet_free(&packet);
		break;
	case Type::SUBTITLE:
		_subtitle.push_back(packet);
		break;
	}
}


optional<std::pair<PacketQueue::Packet, PacketQueue::Type>>
SubtitleSyncPacketQueue::get(bool flushing)
{
	if (!_subtitle.empty()) {
		/* Any subtitle packets we have get returned first */
		auto packet = _subtitle.front();
		_subtitle.pop_front();
		return std::make_pair(Packet(packet), Type::SUBTITLE);
	}

	if (_other.size() > 4096) {
		LOG_WARNING("SubtitleSyncPacketQueue is getting large: {} (_num_video={})", _other.size(), _num_video);
	}

	if ((!flushing && _num_video < 48 && _other.size() < 8192) || _other.empty()) {
		/* We haven't queued up enough video yet, or we don't have anything */
		return boost::none;
	}

	auto packet = _other.front();
	if (packet.second == Type::VIDEO) {
		--_num_video;
	}
	_other.pop_front();
	return packet;
}


void
SubtitleSyncPacketQueue::clear()
{
	for (auto i: _other) {
		if (auto packet = boost::get<AVPacket*>(&i.first)) {
			av_packet_free(packet);
		}
	}
	for (auto i: _subtitle) {
		av_packet_free(&i);
	}
	_other.clear();
	_subtitle.clear();
	_num_video = 0;
}

