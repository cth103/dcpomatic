/*
    Copyright (C) 2015-2021 Carl Hetherington <cth@carlh.net>

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
#include <dcp/array_data.h>
#include <dcp/util.h>
#include <boost/thread/mutex.hpp>


namespace dcp {
	class MonoJ2KPictureFrame;
	class StereoJ2KPictureFrame;
}


class J2KImageProxy : public ImageProxy
{
public:
	J2KImageProxy (boost::filesystem::path path, dcp::Size, AVPixelFormat pixel_format);

	J2KImageProxy (
		std::shared_ptr<const dcp::MonoJ2KPictureFrame> frame,
		dcp::Size,
		AVPixelFormat pixel_format,
		boost::optional<int> forced_reduction
		);

	J2KImageProxy (
		std::shared_ptr<const dcp::StereoJ2KPictureFrame> frame,
		dcp::Size,
		dcp::Eye,
		AVPixelFormat pixel_format,
		boost::optional<int> forced_reduction
		);

	J2KImageProxy (std::shared_ptr<cxml::Node> xml, std::shared_ptr<Socket> socket);

	/* For tests */
	J2KImageProxy (dcp::ArrayData data, dcp::Size size, AVPixelFormat pixel_format);

	Result image (
		Image::Alignment alignment,
		boost::optional<dcp::Size> size = boost::optional<dcp::Size> ()
		) const override;

	void add_metadata(xmlpp::Element*) const override;
	void write_to_socket (std::shared_ptr<Socket> override) const override;
	/** @return true if our image is definitely the same as another, false if it is probably not */
	bool same (std::shared_ptr<const ImageProxy>) const override;
	int prepare (Image::Alignment alignment, boost::optional<dcp::Size> = boost::optional<dcp::Size>()) const override;

	std::shared_ptr<const dcp::Data> j2k () const {
		return _data;
	}

	dcp::Size size () const {
		return _size;
	}

	boost::optional<dcp::Eye> eye () const {
		return _eye;
	}

	size_t memory_used () const override;

private:
	std::shared_ptr<const dcp::Data> _data;
	dcp::Size _size;
	boost::optional<dcp::Eye> _eye;
	mutable std::shared_ptr<Image> _image;
	mutable boost::optional<dcp::Size> _target_size;
	mutable boost::optional<int> _reduce;
	AVPixelFormat _pixel_format;
	mutable boost::mutex _mutex;
	boost::optional<int> _forced_reduction;
	/** true if an error occurred while decoding the JPEG2000 data, false if not */
	mutable bool _error;
};
