/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "image_proxy.h"

class MagickImageProxy : public ImageProxy
{
public:
	MagickImageProxy (boost::filesystem::path);
	MagickImageProxy (boost::shared_ptr<cxml::Node> xml, boost::shared_ptr<Socket> socket);

	boost::shared_ptr<Image> image () const;
	void add_metadata (xmlpp::Node *) const;
	void send_binary (boost::shared_ptr<Socket>) const;
	bool same (boost::shared_ptr<const ImageProxy> other) const;

private:	
	Magick::Blob _blob;
	mutable boost::shared_ptr<Image> _image;
};
