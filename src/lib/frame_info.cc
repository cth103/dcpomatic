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


#include "frame_info.h"
#include <string>


using std::shared_ptr;
using std::string;


J2KFrameInfo::J2KFrameInfo(uint64_t offset_, uint64_t size_, string hash_)
	: dcp::J2KFrameInfo(offset_, size_, hash_)
{

}


J2KFrameInfo::J2KFrameInfo(dcp::J2KFrameInfo const& info)
	: dcp::J2KFrameInfo(info)
{

}


J2KFrameInfo::J2KFrameInfo(dcp::File& info_file, Frame frame, Eyes eyes)
{
	info_file.seek(position(frame, eyes), SEEK_SET);
	info_file.checked_read(&offset, sizeof(offset));
	info_file.checked_read(&size, sizeof(size));

	char hash_buffer[33];
	info_file.checked_read(hash_buffer, 32);
	hash_buffer[32] = '\0';
	hash = hash_buffer;
}


long
J2KFrameInfo::position(Frame frame, Eyes eyes) const
{
	switch (eyes) {
	case Eyes::BOTH:
		return frame * _size_on_disk;
	case Eyes::LEFT:
		return frame * _size_on_disk * 2;
	case Eyes::RIGHT:
		return frame * _size_on_disk * 2 + _size_on_disk;
	default:
		DCPOMATIC_ASSERT(false);
	}

	DCPOMATIC_ASSERT(false);
}


/** @param frame reel-relative frame */
void
J2KFrameInfo::write(dcp::File& info_file, Frame frame, Eyes eyes) const
{
	info_file.seek(position(frame, eyes), SEEK_SET);
	info_file.checked_write(&offset, sizeof(offset));
	info_file.checked_write(&size, sizeof(size));
	info_file.checked_write(hash.c_str(), hash.size());
}

