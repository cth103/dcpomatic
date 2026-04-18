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


/** @file  src/lib/subtitle_sync_packet_queue.h
 *  @brief SubtitleSyncPacketQueue class.
 */


#include "packet_queue.h"
#include <boost/variant.hpp>
#include <cstdint>
#include <deque>

struct AVPacket;


/** @class PacketQueue
 *  @brief A queue of FFmpeg packets, used to re-order them so that
 *  subtitles do not arrive too late.
 */
class SubtitleSyncPacketQueue : public PacketQueue
{
public:
	/** Add a packet to the queue.  Does not ref the packet; we expect
	 *  the packet to be freed when it comes out of get() (or by clear()).
	 */
	void add(AVPacket* packet, Type type) override;

	/** Get the next packet to process.
	 *  @param flushing should be true if we are flushing at the end of a decode.
	 *  When this is true the queue will be emptied without trying to re-order it.
	 *  Returns boost::none when there are no more packets to get.
	 */
	boost::optional<std::pair<Packet, Type>> get(bool flushing) override;

	/** Clear the queue.  Packets will be freed. */
	void clear() override;

private:
	int _num_video = 0;
	std::deque<AVPacket*> _subtitle;
	std::deque<std::pair<Packet, Type>> _other;
};

