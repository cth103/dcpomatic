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

/** @file  src/lib/image_proxy.h
 *  @brief ImageProxy and subclasses.
 */

#include <boost/shared_ptr.hpp>
#include <boost/filesystem.hpp>
#include <Magick++.h>
#include <libxml++/libxml++.h>

class Image;
class Socket;

namespace cxml {
	class Node;
}

/** @class ImageProxy
 *  @brief A class which holds an Image, and can produce it on request.
 *
 *  This is so that decoding of source images can be postponed until
 *  the encoder thread, where multi-threading is happening, instead
 *  of happening in a single-threaded decoder.
 *
 *  For example, large TIFFs are slow to decode, so this class will keep
 *  the TIFF data TIFF until such a time that the actual image is needed.
 *  At this point, the class decodes the TIFF to an Image.
 */
class ImageProxy
{
public:
	virtual boost::shared_ptr<Image> image () const = 0;
	virtual void add_metadata (xmlpp::Node *) const = 0;
	virtual void send_binary (boost::shared_ptr<Socket>) const = 0;
};

class RawImageProxy : public ImageProxy
{
public:
	RawImageProxy (boost::shared_ptr<Image>);
	RawImageProxy (boost::shared_ptr<cxml::Node> xml, boost::shared_ptr<Socket> socket);

	boost::shared_ptr<Image> image () const;
	void add_metadata (xmlpp::Node *) const;
	void send_binary (boost::shared_ptr<Socket>) const;
	
private:
	boost::shared_ptr<Image> _image;
};

class MagickImageProxy : public ImageProxy
{
public:
	MagickImageProxy (boost::filesystem::path);
	MagickImageProxy (boost::shared_ptr<cxml::Node> xml, boost::shared_ptr<Socket> socket);

	boost::shared_ptr<Image> image () const;
	void add_metadata (xmlpp::Node *) const;
	void send_binary (boost::shared_ptr<Socket>) const;

private:	
	Magick::Blob _blob;
	mutable boost::shared_ptr<Image> _image;
};

boost::shared_ptr<ImageProxy> image_proxy_factory (boost::shared_ptr<cxml::Node> xml, boost::shared_ptr<Socket> socket);
