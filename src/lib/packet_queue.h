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


#ifndef DCPOMATIC_PACKET_QUEUE_H
#define DCPOMATIC_PACKET_QUEUE_H


/** @file  src/lib/packet_queue.h
 *  @brief PacketQueue parent class.
 */


#include <boost/optional.hpp>
#include <boost/variant.hpp>
#include <cstdint>
#include <deque>

struct AVPacket;


/** @class PacketQueue
 *  @brief Parent class for things which take and then return AVPackets, possibly
 *  re-ordering them.
 */
class PacketQueue
{
public:
	virtual ~PacketQueue() = default;

	enum class Type {
		VIDEO,
		AUDIO,
		SUBTITLE,
		DROP,
	};

	/** Container for the only information we need to keep from a DROP packet */
	struct PacketInfo {
		PacketInfo(AVPacket* packet);

		int stream_index;
		int64_t dts;
	};

	typedef boost::variant<AVPacket*, PacketInfo> Packet;

	/** Add a packet to the queue.  Does not ref the packet; we expect
	 *  the packet to be freed when it comes out of get() (or by clear()).
	 */
	virtual void add(AVPacket* packet, Type type) = 0;

	/** Get the next packet to process.
	 *  @param flushing should be true if we are flushing at the end of a decode.
	 *  When this is true the queue will be emptied without trying to re-order it.
	 *  Returns boost::none when there are no more packets to get.
	 */
	virtual boost::optional<std::pair<Packet, Type>> get(bool flushing) = 0;

	/** Clear the queue.  Packets will be freed. */
	virtual void clear() = 0;
};


#endif

