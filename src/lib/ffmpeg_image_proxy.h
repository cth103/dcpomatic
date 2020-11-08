/*
    Copyright (C) 2014-2018 Carl Hetherington <cth@carlh.net>

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

#include "image_proxy.h"
#include "types.h"
#include <dcp/array_data.h>
#include <boost/thread/mutex.hpp>
#include <boost/filesystem.hpp>

class FFmpegImageProxy : public ImageProxy
{
public:
	explicit FFmpegImageProxy (boost::filesystem::path, VideoRange video_range);
	explicit FFmpegImageProxy (dcp::ArrayData, VideoRange video_range);
	FFmpegImageProxy (boost::shared_ptr<cxml::Node> xml, boost::shared_ptr<Socket> socket);

	Result image (
		boost::optional<dcp::Size> size = boost::optional<dcp::Size> ()
		) const;

	void add_metadata (xmlpp::Node *) const;
	void write_to_socket (boost::shared_ptr<Socket>) const;
	bool same (boost::shared_ptr<const ImageProxy> other) const;
	size_t memory_used () const;

	int avio_read (uint8_t* buffer, int const amount);
	int64_t avio_seek (int64_t const pos, int whence);

private:
	dcp::ArrayData _data;
	VideoRange _video_range;
	mutable int64_t _pos;
	/** Path of a file that this image came from, if applicable; stored so that
	    failed-decode errors can give more detail.
	*/
	boost::optional<boost::filesystem::path> _path;
	mutable boost::shared_ptr<Image> _image;
	mutable boost::mutex _mutex;
};
