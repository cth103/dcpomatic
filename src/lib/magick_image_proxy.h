/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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
#include <Magick++.h>
#include <boost/thread/mutex.hpp>
#include <boost/filesystem.hpp>

class MagickImageProxy : public ImageProxy
{
public:
	MagickImageProxy (boost::filesystem::path);
	MagickImageProxy (boost::shared_ptr<cxml::Node> xml, boost::shared_ptr<Socket> socket);

	std::pair<boost::shared_ptr<Image>, int> image (
		boost::optional<dcp::NoteHandler> note = boost::optional<dcp::NoteHandler> (),
		boost::optional<dcp::Size> size = boost::optional<dcp::Size> ()
		) const;

	void add_metadata (xmlpp::Node *) const;
	void send_binary (boost::shared_ptr<Socket>) const;
	bool same (boost::shared_ptr<const ImageProxy> other) const;
	AVPixelFormat pixel_format () const;
	size_t memory_used () const;

private:
	Magick::Blob _blob;
	mutable boost::shared_ptr<Image> _image;
	mutable boost::mutex _mutex;
};
