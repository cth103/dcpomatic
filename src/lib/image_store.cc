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


#include "image.h"
#include "image_store.h"
extern "C" {
#include <libavutil/buffer.h>
}


using std::shared_ptr;


void
ImageStore::buffer_free(void* opaque, uint8_t* data)
{
	reinterpret_cast<ImageStore*>(opaque)->buffer_free2(data);
}


void
ImageStore::buffer_free2(uint8_t* data)
{
	boost::mutex::scoped_lock lm(_mutex);
	auto iter = _images.find(data);
	if (iter != _images.end()) {
		iter->second.second--;
		if (iter->second.second == 0) {
			_images.erase(data);
		}
	}
}


AVBufferRef*
ImageStore::create_buffer(shared_ptr<const Image> image, int component)
{
	boost::mutex::scoped_lock lm(_mutex);
	auto key = image->data()[component];
	auto iter = _images.find(key);
	if (iter != _images.end()) {
		iter->second.second++;
	} else {
		_images[key] = { image, 1 };
	}

	return av_buffer_create(image->data()[component], image->stride()[component] * image->size().height, &buffer_free, this, 0);
}
