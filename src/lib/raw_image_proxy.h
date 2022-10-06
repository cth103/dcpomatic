/*
    Copyright (C) 2014-2021 Carl Hetherington <cth@carlh.net>

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


#ifndef DCPOMATIC_RAW_IMAGE_PROXY_H
#define DCPOMATIC_RAW_IMAGE_PROXY_H


#include "image_proxy.h"


class RawImageProxy : public ImageProxy
{
public:
	explicit RawImageProxy(std::shared_ptr<const Image>);
	RawImageProxy (std::shared_ptr<cxml::Node> xml, std::shared_ptr<Socket> socket);

	Result image (
		Image::Alignment alignment,
		boost::optional<dcp::Size> size = boost::optional<dcp::Size> ()
		) const override;

	void add_metadata (xmlpp::Node *) const override;
	void write_to_socket (std::shared_ptr<Socket>) const override;
	bool same (std::shared_ptr<const ImageProxy>) const override;
	size_t memory_used () const override;

private:
	std::shared_ptr<const Image> _image;
};


#endif
