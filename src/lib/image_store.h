/*
    Copyright (C) 2017-2018 Carl Hetherington <cth@carlh.net>

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


#include <boost/thread.hpp>
#include <map>


struct AVBufferRef;
class Image;


/** Store of shared_ptr<Image> to keep them alive whilst raw pointers into
 *  their data have been passed to FFmpeg.  The second part of the pair is
 *  a count of how many copies of the same key must be kept.
 */
class ImageStore
{
public:
	ImageStore() {}

	ImageStore(ImageStore const&) = delete;
	ImageStore& operator=(ImageStore const&) = delete;

	AVBufferRef* create_buffer(std::shared_ptr<const Image>, int component);

private:

	static void buffer_free(void* opaque, uint8_t* data);
	void buffer_free2(uint8_t* data);

	std::map<uint8_t*, std::pair<std::shared_ptr<const Image>, int>> _images;
	boost::mutex _mutex;
};
