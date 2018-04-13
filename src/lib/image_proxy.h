/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

#ifndef DCPOMATIC_IMAGE_PROXY_H
#define DCPOMATIC_IMAGE_PROXY_H

/** @file  src/lib/image_proxy.h
 *  @brief ImageProxy and subclasses.
 */

extern "C" {
#include <libavutil/pixfmt.h>
}
#include <dcp/types.h>
#include <boost/shared_ptr.hpp>
#include <boost/optional.hpp>
#include <boost/utility.hpp>

class Image;
class Socket;

namespace xmlpp {
	class Node;
}

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
 *  the TIFF data compressed until the decompressed image is needed.
 *  At this point, the class decodes the TIFF to an Image.
 */
class ImageProxy : public boost::noncopyable
{
public:
	virtual ~ImageProxy () {}

	/** @param note Handler for any notes that occur.
	 *  @param size Size that the returned image will be scaled to, in case this
	 *  can be used as an optimisation.
	 *  @return Image (which must be aligned) and log2 of any scaling down that has
	 *  already been applied to the image; e.g. if the the image is already half the size
	 *  of the original, the second part of the return value will be 1.
	 */
	virtual std::pair<boost::shared_ptr<Image>, int> image (
		boost::optional<dcp::NoteHandler> note = boost::optional<dcp::NoteHandler> (),
		boost::optional<dcp::Size> size = boost::optional<dcp::Size> ()
		) const = 0;

	virtual void add_metadata (xmlpp::Node *) const = 0;
	virtual void send_binary (boost::shared_ptr<Socket>) const = 0;
	/** @return true if our image is definitely the same as another, false if it is probably not */
	virtual bool same (boost::shared_ptr<const ImageProxy>) const = 0;
	/** Do any useful work that would speed up a subsequent call to ::image().
	 *  This method may be called in a different thread to image().
	 *  @return log2 of any scaling down that will be applied to the image.
	 */
	int prepare (boost::optional<dcp::Size> = boost::optional<dcp::Size>()) const { return 0; }
	virtual AVPixelFormat pixel_format () const = 0;
	virtual size_t memory_used () const = 0;
};

boost::shared_ptr<ImageProxy> image_proxy_factory (boost::shared_ptr<cxml::Node> xml, boost::shared_ptr<Socket> socket);

#endif
