/*
    Copyright (C) 2015 Carl Hetherington <cth@carlh.net>

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
#include <dcp/util.h>
#include <dcp/data.h>

namespace dcp {
	class MonoPictureFrame;
	class StereoPictureFrame;
}

class J2KImageProxy : public ImageProxy
{
public:
	J2KImageProxy (boost::filesystem::path path, dcp::Size, AVPixelFormat pixel_format);
	J2KImageProxy (boost::shared_ptr<const dcp::MonoPictureFrame> frame, dcp::Size, AVPixelFormat pixel_format);
	J2KImageProxy (boost::shared_ptr<const dcp::StereoPictureFrame> frame, dcp::Size, dcp::Eye, AVPixelFormat pixel_format);
	J2KImageProxy (boost::shared_ptr<cxml::Node> xml, boost::shared_ptr<Socket> socket);

	boost::shared_ptr<Image> image (
		boost::optional<dcp::NoteHandler> note = boost::optional<dcp::NoteHandler> (),
		boost::optional<dcp::Size> size = boost::optional<dcp::Size> ()
		) const;

	void add_metadata (xmlpp::Node *) const;
	void send_binary (boost::shared_ptr<Socket>) const;
	/** @return true if our image is definitely the same as another, false if it is probably not */
	bool same (boost::shared_ptr<const ImageProxy>) const;
	AVPixelFormat pixel_format () const {
		return _pixel_format;
	}

	dcp::Data j2k () const {
		return _data;
	}

	dcp::Size size () const {
		return _size;
	}

private:
	friend struct client_server_test_j2k;

	/* For tests */
	J2KImageProxy (dcp::Data data, dcp::Size size, AVPixelFormat pixel_format);

	dcp::Data _data;
	dcp::Size _size;
	boost::optional<dcp::Eye> _eye;
	mutable boost::shared_ptr<dcp::OpenJPEGImage> _decompressed;
	mutable boost::optional<dcp::Size> _target_size;
	AVPixelFormat _pixel_format;
};
